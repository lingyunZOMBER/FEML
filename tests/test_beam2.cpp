#include "feml/element/beam2_timoshenko.hpp"
#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <limits>

using Catch::Matchers::WithinRel;

namespace {

	/// 矩形截面悬臂梁
	struct CantileverModel {
		double b;      ///< 截面宽
		double h;      ///< 截面高
		double L;      ///< 梁长
		feml::IntegrationScheme scheme;

		feml::Material material;
		feml::Section section;
		feml::ElementContext ctx;

		CantileverModel(double b_, double h_, double L_, double E_, double nu_,
			feml::IntegrationScheme s)
			: b(b_), h(h_), L(L_), scheme(s)
		{
			material.id = 1;
			material.E = E_;
			material.nu = nu_;

			section.id = 1;
			section.area = b * h;
			section.inertia = b * h * h * h / 12.0;
			section.shearFactor = 5.0 / 6.0;

			ctx.node_coords.resize(2, 1);
			ctx.node_coords << 0.0, L;
			ctx.material = &material;
			ctx.section = &section;
			ctx.scheme = scheme;
		}

		double EI()  const { return material.E * section.inertia; }
		double kGA() const { return section.shearFactor * material.shearModulus() * section.area; }

		/// Φ = 12EI / (k_s·GA·L²) = E·h² / (k_s·G·L²)
		double phi() const { return 12.0 * EI() / (kGA() * L * L); }
	};

	/// 解 1 个单元的悬臂梁。
	///
	/// 节点 1 固支 → w₁ = θ₁ = 0，所以自由自由度正好是 [w₂, θ₂]，
	/// 也就是 K 的右下 2×2 子块。直接取子块求解即可，

	/// @return [w₂, θ₂]
	Eigen::Vector2d solveCantilever(const Eigen::MatrixXd& K,
		double tipLoad, double tipMoment) {
		const Eigen::Matrix2d Kff = K.bottomRightCorner<2, 2>();

		Eigen::Vector2d f;
		f << tipLoad, tipMoment;

		return Kff.ldlt().solve(f);
	}

	/// 数出接近零的特征值个数（相对容差）。
	/// 对刚度阵来说，零特征值的个数 = 刚体模态数 + 任何"零能模态"。
	int zeroEigenvalueCount(const Eigen::MatrixXd& K) {
		Eigen::SelfAdjointEigenSolver<Eigen::MatrixXd> es(K);
		const Eigen::VectorXd ev = es.eigenvalues().cwiseAbs();

		const double scale = ev.maxCoeff();
		if (scale <= 0.0) {
			return static_cast<int>(ev.size());
		}

		int count = 0;
		for (Eigen::Index i = 0; i < ev.size(); ++i) {
			if (ev(i) < scale * 1e-10) {
				++count;
			}
		}
		return count;
	}

	constexpr feml::IntegrationScheme kAllSchemes[] = {
		feml::IntegrationScheme::Full,
		feml::IntegrationScheme::Reduced,
		feml::IntegrationScheme::SelectiveReduced,
	};

} // namespace

// ===========================================================================
//  第一部分：不依赖具体数值的结构性质
// ===========================================================================

TEST_CASE("Beam2Timoshenko reports its basic information", "[beam2]") {
	const feml::Beam2Timoshenko beam;

	REQUIRE(beam.name() == "BEAM2");
	REQUIRE(beam.nodeCount() == 2);
	REQUIRE(beam.dofPerNode() == 2);
	REQUIRE(beam.spatialDim() == 1);
	REQUIRE(beam.totalDof() == 4);
	REQUIRE(beam.dofNames().size() == 2);
}

TEST_CASE("beam stiffness is symmetric", "[beam2]") {
	const feml::Beam2Timoshenko beam;

	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		const CantileverModel m(0.1, 0.05, 2.0, 200.0e9, 0.3, scheme);

		Eigen::MatrixXd K;
		beam.stiffness(m.ctx, K);

		REQUIRE(K.rows() == 4);
		REQUIRE(K.cols() == 4);
		REQUIRE((K - K.transpose()).norm() < K.norm() * 1e-14);
	}
}

TEST_CASE("beam stiffness annihilates rigid body modes", "[beam2]") {
	const feml::Beam2Timoshenko beam;
	const CantileverModel m(0.1, 0.05, 2.0, 200.0e9, 0.3,
		feml::IntegrationScheme::Full);

	Eigen::MatrixXd K;
	beam.stiffness(m.ctx, K);

	const double tol = K.norm() * 1e-14;

	// 刚体平移：两个节点同向平移，转角不变
	Eigen::Vector4d translation;
	translation << 1.0, 0.0, 1.0, 0.0;
	REQUIRE((K * translation).norm() < tol);

	// 刚体转动：绕节点 1 转单位角度
	// w 按 x 线性变化（节点 2 的 w = L），θ 处处相等
	Eigen::Vector4d rotation;
	rotation << 0.0, 1.0, m.L, 1.0;
	REQUIRE((K * rotation).norm() < tol);
}

TEST_CASE("no scheme produces spurious zero-energy modes", "[beam2]") {
	const feml::Beam2Timoshenko beam;

	// 4 个自由度 − 2 个刚体模态 = 秩应为 2，即零特征值恰好 2 个。
	//
	// 这条对【三种方案都成立】，包括减缩积分。这一点很重要：
	// 2 节点梁单元用 1 点积分不会产生"零能模态"（hourglass），
	// 所以它是安全的。而 4 节点四边形单元用 1 点积分就会产生 hourglass——
	// 那才需要额外的稳定化手段。
	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		const CantileverModel m(0.1, 0.05, 2.0, 200.0e9, 0.3, scheme);

		Eigen::MatrixXd K;
		beam.stiffness(m.ctx, K);

		REQUIRE(zeroEigenvalueCount(K) == 2);
	}
}

// ===========================================================================
//  第二部分：离散解 vs 解析解
// ===========================================================================

TEST_CASE("reduced integration gives exact response under pure end moment",
	"[beam2][locking]") {
	// 这是作业里"1 个单元的结果应该和书上算例相同"的那一条。
	//
	// 纯弯（端部弯矩 M）下解析解：θ = ML/EI，w = ML²/(2EI)
	// 减缩积分 1 个单元给出【完全精确】的结果，与细长比无关。
	const feml::Beam2Timoshenko beam;
	const double M = 1000.0;

	for (const feml::IntegrationScheme scheme : {
		feml::IntegrationScheme::Reduced,
			feml::IntegrationScheme::SelectiveReduced }) {

		for (const double hOverL : { 0.5, 0.2, 0.1, 0.05, 0.01, 0.001 }) {
			const double L = 1.0;
			const double b = 0.1;
			const double h = hOverL * L;

			const CantileverModel m(b, h, L, 200.0e9, 0.3, scheme);

			Eigen::MatrixXd K;
			beam.stiffness(m.ctx, K);

			const Eigen::Vector2d u = solveCantilever(K, 0.0, M);

			const double wExact = M * L * L / (2.0 * m.EI());
			const double tExact = M * L / m.EI();

			// 容差必须随细长比放宽 —— 这不是给错误找借口，而是浮点条件的客观限制。
			//
			// 单元在【数学上】是精确的，但 h/L → 0 时 K 的条件数按 (h/L)^-2 恶化：
			// 对角元 β/L 与 αL/4 相差约 1/Φ 个量级，消元时的相消误差约为 eps/Φ。
			// 实测 h/L = 0.001（Φ ≈ 3.1e-6）时相对误差约 5e-11，
			// 与 eps/Φ ≈ 7e-11 的量级估计吻合。
			//
			// 对比一下：完全积分的锁死误差是 O(1) 量级（剩下百分之几的挠度）。
			// 1e-10 与 1 之间的差距，让"精确"和"锁死"在数值上毫无混淆的可能。
			const double eps = std::numeric_limits<double>::epsilon();
			const double tol = std::max(1.0e-12, 10.0 * eps / m.phi());

			INFO("h/L = " << hOverL << ", Phi = " << m.phi() << ", tol = " << tol);
			REQUIRE_THAT(u(0), WithinRel(wExact, tol));
			REQUIRE_THAT(u(1), WithinRel(tExact, tol));
		}
	}
}

TEST_CASE("tip rotation under concentrated load is exact with reduced integration",
	"[beam2][locking]") {
	// 端部集中力 P 下，减缩积分的端部转角仍是精确的：θ = PL²/(2EI)
	// （但挠度不是精确的，见下一个用例）
	const feml::Beam2Timoshenko beam;
	const double P = 1000.0;
	const double L = 1.0;
	const double b = 0.1;

	for (const double hOverL : { 0.2, 0.1, 0.01, 0.001 }) {
		const CantileverModel m(b, hOverL * L, L, 200.0e9, 0.3,
			feml::IntegrationScheme::Reduced);

		Eigen::MatrixXd K;
		beam.stiffness(m.ctx, K);

		const Eigen::Vector2d u = solveCantilever(K, P, 0.0);

		const double tExact = P * L * L / (2.0 * m.EI());

		INFO("h/L = " << hOverL);
		REQUIRE_THAT(u(1), WithinRel(tExact, 1e-10));
	}
}

TEST_CASE("full integration locks as the beam becomes slender", "[beam2][locking]") {
	// 完全积分的解 = 解析解 / (1 + 1/Φ)。
	//
	// 这是剪切锁死的定量刻画：h/L → 0 时 Φ → 0，(1 + 1/Φ) → ∞，
	// 于是挠度趋于 0 —— 梁变得无限刚，数值解彻底失真。
	//
	// 这个关系是手推的，测试把它变成可证伪的断言。
	const feml::Beam2Timoshenko beam;
	const double P = 1000.0;
	const double L = 1.0;
	const double b = 0.1;

	double previousRatio = 2.0;

	for (const double hOverL : { 0.2, 0.1, 0.05, 0.02, 0.01 }) {
		const CantileverModel m(b, hOverL * L, L, 200.0e9, 0.3,
			feml::IntegrationScheme::Full);

		Eigen::MatrixXd K;
		beam.stiffness(m.ctx, K);

		const Eigen::Vector2d u = solveCantilever(K, P, 0.0);

		const double wExact = P * L * L * L / (3.0 * m.EI()) + P * L / m.kGA();
		const double ratio = u(0) / wExact;

		INFO("h/L = " << hOverL << ", Phi = " << m.phi() << ", ratio = " << ratio);

		// (1) 数值解总是偏刚（锁死方向）
		REQUIRE(ratio < 1.0);

		// (2) 随梁变细，比值单调下降
		REQUIRE(ratio < previousRatio);

		// (3) 定量关系：ratio = 1 / (1 + 1/Φ)
		REQUIRE_THAT(ratio, WithinRel(1.0 / (1.0 + 1.0 / m.phi()), 1e-10));

		previousRatio = ratio;
	}

	// 最细的那一根（h/L = 0.01）：挠度只剩下解析解的万分之几
	REQUIRE(previousRatio < 1e-2);
}

TEST_CASE("reduced integration does not lock", "[beam2][locking]") {
	// 减缩积分的挠度比收敛到 3/4，而不是 0。
	//
	// 预测公式（手推）：ratio = (1/4 + Φ/12) / (1/3 + Φ/12)
	//     Φ → 0（细长梁）  →  (1/4)/(1/3) = 3/4
	//     Φ → ∞（粗短梁）  →  1
	//
	// 也就是说减缩积分有 25% 的系统偏刚（弯曲项被积成 PL³/4EI 而不是
	// PL³/3EI），但【误差有界】，不会随细长比恶化。这就是它与完全积分的本质区别。
	const feml::Beam2Timoshenko beam;
	const double P = 1000.0;
	const double L = 1.0;
	const double b = 0.1;

	double previousRatio = 0.0;   // 从粗到细，比值应从 1 递减到 3/4

	for (const double hOverL : { 0.5, 0.3, 0.1, 0.05, 0.01, 0.001 }) {
		const CantileverModel m(b, hOverL * L, L, 200.0e9, 0.3,
			feml::IntegrationScheme::Reduced);

		Eigen::MatrixXd K;
		beam.stiffness(m.ctx, K);

		const Eigen::Vector2d u = solveCantilever(K, P, 0.0);

		const double wExact = P * L * L * L / (3.0 * m.EI()) + P * L / m.kGA();
		const double ratio = u(0) / wExact;

		const double phi = m.phi();
		const double predicted = (0.25 + phi / 12.0) / (1.0 / 3.0 + phi / 12.0);

		INFO("h/L = " << hOverL << ", Phi = " << phi
			<< ", ratio = " << ratio << ", predicted = " << predicted);

		REQUIRE_THAT(ratio, WithinRel(predicted, 1e-10));

		// 不会锁死：比值始终有下界
		REQUIRE(ratio > 0.7);
		previousRatio = ratio;
	}

	// 细长极限下收敛到 3/4
	REQUIRE_THAT(previousRatio, WithinRel(0.75, 1e-6));
}