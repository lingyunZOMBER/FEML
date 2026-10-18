#include "feml/analysis/assembler.hpp"
#include "feml/analysis/static_analysis.hpp"
#include "feml/core/exceptions.hpp"
#include "feml/element/beam3_timoshenko.hpp"
#include "feml/element/element.hpp"
#include "feml/element/register_builtin.hpp"
#include "feml/linalg/eigen_solver.hpp"
#include "feml/model/dof_manager.hpp"
#include "feml/model/material.hpp"
#include "feml/model/mesh.hpp"
#include "feml/model/section.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include <algorithm>
#include <cmath>
#include <limits>
#include <string>
#include <vector>

using Catch::Matchers::WithinRel;

namespace {

constexpr double kE      = 200.0e9;
constexpr double kNu     = 0.3;
constexpr double kWidth  = 0.1;
constexpr double kHeight = 0.1;
constexpr double kLength = 2.0;

/// 生成 n 个等长 BEAM3 单元组成的悬臂梁网格。
///
/// 注意节点数：每个 3 节点单元跨两个子区间，所以 n 个单元的梁
/// 有 2n+1 个节点。
feml::Mesh makeMesh3(int elementCount, double L) {
	feml::Mesh mesh;

	const int nodeCount = 2 * elementCount + 1;
	mesh.node_coords.resize(nodeCount, 1);
	for (int i = 0; i < nodeCount; ++i) {
		mesh.node_coords(i, 0) = L * static_cast<double>(i)
		                       / static_cast<double>(nodeCount - 1);
	}

	mesh.element_type.assign(static_cast<std::size_t>(elementCount), "BEAM3");
	mesh.element_nodes.resize(static_cast<std::size_t>(elementCount));
	for (int e = 0; e < elementCount; ++e) {
		mesh.element_nodes[static_cast<std::size_t>(e)] = std::vector<feml::NodeId>{
			static_cast<feml::NodeId>(2 * e),
			static_cast<feml::NodeId>(2 * e + 1),
			static_cast<feml::NodeId>(2 * e + 2)
		};
	}

	return mesh;
}

feml::Material makeMaterial() {
	feml::Material m;
	m.id = 1;
	m.E  = kE;
	m.nu = kNu;
	return m;
}

feml::Section makeSection(double h) {
	feml::Section s;
	s.id          = 1;
	s.area        = kWidth * h;
	s.inertia     = kWidth * h * h * h / 12.0;
	s.shearFactor = 5.0 / 6.0;
	return s;
}

double bendingStiffness(const feml::Section& s) { return kE * s.inertia; }
double shearStiffness(const feml::Material& m, const feml::Section& s) {
	return s.shearFactor * m.shearModulus() * s.area;
}

constexpr feml::IntegrationScheme kAllSchemes[] = {
	feml::IntegrationScheme::Full,
	feml::IntegrationScheme::Reduced,
	feml::IntegrationScheme::SelectiveReduced,
};

feml::ElementContext makeContext(const feml::Mesh& mesh, const feml::Material& material,
                                 const feml::Section& section,
                                 feml::IntegrationScheme scheme) {
	feml::ElementContext ctx;
	ctx.node_coords = mesh.node_coords.topRows(3);
	ctx.material    = &material;
	ctx.section     = &section;
	ctx.scheme      = scheme;
	return ctx;
}

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

/// 悬臂梁的端部响应（用完整流程求解）
struct TipResponse {
	double w = 0.0;
	double theta = 0.0;
};

TipResponse solveCantilever3(const feml::Mesh& mesh, const feml::Material& material,
                             const feml::Section& section,
                             feml::IntegrationScheme scheme,
                             double tipLoad, double tipMoment) {
	feml::DofManager dofs(mesh.nodeCount(), 2);
	dofs.constrain(0, 0);
	dofs.constrain(0, 1);
	dofs.number();

	feml::StaticProblem problem;
	problem.mesh     = &mesh;
	problem.material = &material;
	problem.section  = &section;
	problem.dofs     = &dofs;
	problem.scheme   = scheme;

	const int last = mesh.nodeCount() - 1;
	problem.nodalLoads = Eigen::VectorXd::Zero(2 * mesh.nodeCount());
	problem.nodalLoads(last * 2 + 0) = tipLoad;
	problem.nodalLoads(last * 2 + 1) = tipMoment;

	feml::EigenLdltSolver solver;
	const feml::StaticResult result = feml::solveStatic(problem, solver);

	return TipResponse{ result.displacement(last * 2 + 0),
	                    result.displacement(last * 2 + 1) };
}

} // namespace

// ===========================================================================
//  不依赖具体数值的结构性质
// ===========================================================================

TEST_CASE("Beam3Timoshenko reports its basic information", "[beam3]") {
	const feml::Beam3Timoshenko beam;

	REQUIRE(beam.name() == "BEAM3");
	REQUIRE(beam.nodeCount() == 3);
	REQUIRE(beam.dofPerNode() == 2);
	REQUIRE(beam.spatialDim() == 1);
	REQUIRE(beam.totalDof() == 6);
	REQUIRE(beam.dofNames().size() == 2);
}

TEST_CASE("beam3 stiffness is symmetric", "[beam3]") {
	const feml::Beam3Timoshenko beam;

	const feml::Mesh mesh = makeMesh3(1, kLength);
	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(kHeight);

	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		const feml::ElementContext ctx =
			makeContext(mesh, material, section, scheme);

		Eigen::MatrixXd K;
		beam.stiffness(ctx, K);

		REQUIRE(K.rows() == 6);
		REQUIRE(K.cols() == 6);
		REQUIRE((K - K.transpose()).norm() < K.norm() * 1e-14);
	}
}

TEST_CASE("beam3 stiffness annihilates rigid body modes", "[beam3]") {
	const feml::Beam3Timoshenko beam;

	const feml::Mesh mesh = makeMesh3(1, kLength);
	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(kHeight);

	feml::ElementContext ctx =
		makeContext(mesh, material, section, feml::IntegrationScheme::Full);

	Eigen::MatrixXd K;
	beam.stiffness(ctx, K);

	const double tol = K.norm() * 1e-13;

	// 刚体平移：三个节点同向平移，转角不变
	Eigen::VectorXd translation = Eigen::VectorXd::Zero(6);
	translation << 1.0, 0.0, 1.0, 0.0, 1.0, 0.0;
	REQUIRE((K * translation).norm() < tol);

	// 刚体转动：绕节点 1 转单位角度
	// w 按 x 线性变化，θ 处处相等
	const double L = kLength;
	Eigen::VectorXd rotation = Eigen::VectorXd::Zero(6);
	rotation << 0.0, 1.0, L / 2.0, 1.0, L, 1.0;
	REQUIRE((K * rotation).norm() < tol);
}

TEST_CASE("beam3 has exactly two zero eigenvalues for every scheme", "[beam3]") {
	const feml::Beam3Timoshenko beam;

	const feml::Mesh mesh = makeMesh3(1, kLength);
	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(kHeight);

	// 6 个自由度 − 2 个刚体模态 = 秩应为 4，即零特征值恰好 2 个。
	// 三种方案都成立 —— 说明没有任何方案引入零能模态（hourglass）。
	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		const feml::ElementContext ctx =
			makeContext(mesh, material, section, scheme);

		Eigen::MatrixXd K;
		beam.stiffness(ctx, K);

		REQUIRE(zeroEigenvalueCount(K) == 2);
	}
}

// ===========================================================================
//  离散解 vs 解析解
// ===========================================================================

TEST_CASE("beam3 pure end moment is exact for every mesh and scheme",
          "[beam3][locking]") {
	// 3 节点单元有个比 2 节点更强的性质：
	// 纯弯的解析解（θ 线性、w 二次）【完全落在二次插值空间内】，
	// 所以只要积分是精确的，离散解就等于解析解。
	//
	// 减缩积分下"积分是否精确"要单独看：
	//   - 弯曲项：κ 是常数，2 点已精确 ✓
	//   - 剪切项：解析解上 γ ≡ 0，欠积分不影响 ✓
	// 所以三种方案都应该精确。
	const double L = kLength;
	const double M = 1000.0;

	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(kHeight);

	const double EI = bendingStiffness(section);
	const double wExact = M * L * L / (2.0 * EI);
	const double tExact = M * L / EI;

	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		for (const int elementCount : { 1, 2, 4, 8 }) {
			const feml::Mesh mesh = makeMesh3(elementCount, L);

			const TipResponse tip =
				solveCantilever3(mesh, material, section, scheme, 0.0, M);

			INFO("scheme = " << static_cast<int>(scheme)
			                 << ", elementCount = " << elementCount);

			REQUIRE_THAT(tip.w, WithinRel(wExact, 1e-9));
			REQUIRE_THAT(tip.theta, WithinRel(tExact, 1e-9));
		}
	}
}

// ---------------------------------------------------------------------------
//  【重要结论】3 节点单元的完全积分【不】锁死 —— 这与 2 节点单元相反。
//
//  这正是作业"比较 2 节点与 3 节点"要的答案，机制如下：
//
//  完全积分（剪切 3 点）强制 γ≡0（因为 γ 是 ξ 的二次函数，3 个点足以定死它）。
//  γ≡0 意味着 w' = θ，把变形空间限制到「纯弯模式」：
//        w 二次、θ 线性、且 w' = θ
//  这个空间里【仍然有 1 个变形自由度】，所以梁照样能弯 ——
//  只是翘曲模式被压掉了，于是挠度比趋于 3/4 而不是 0。
//
//  对比 2 节点单元：γ 是【线性】的，2 点积分同样强制 γ≡0，但
//  w' = θ 加上 w 线性 ⇒ θ 必须是常数 ⇒ 再施加固定边界后 w₂ = θ = 0，
//  【所有变形自由度都被压掉】—— 这才叫锁死。
//
//  所以"减缩积分"对线性单元是必需的，对二次单元却不是。
// ---------------------------------------------------------------------------
TEST_CASE("beam3 full integration does not lock", "[beam3][locking]") {
	const double P = 1000.0;
	const double L = kLength;

	double previousRatio = 2.0;

	for (const double hOverL : { 0.2, 0.1, 0.05, 0.02, 0.01, 0.001 }) {
		const double h = hOverL * L;

		const feml::Material material = makeMaterial();
		const feml::Section section = makeSection(h);

		const feml::Mesh mesh = makeMesh3(1, L);
		const TipResponse tip = solveCantilever3(
			mesh, material, section, feml::IntegrationScheme::Full, P, 0.0);

		const double EI  = bendingStiffness(section);
		const double kGA = shearStiffness(material, section);
		const double wExact = P * L * L * L / (3.0 * EI) + P * L / kGA;

		const double ratio = tip.w / wExact;

		INFO("h/L = " << hOverL << ", ratio = " << ratio);

		// 数值解偏刚（存活空间只剩纯弯模式），但【有下界】
		REQUIRE(ratio < 1.0);
		REQUIRE(ratio > 0.7);            // ← 这一条就是"不锁死"
		REQUIRE(ratio < previousRatio);

		previousRatio = ratio;
	}

	// 细长极限下收敛到 3/4，而不是 0
	REQUIRE_THAT(previousRatio, WithinRel(0.75, 1e-2));
}

TEST_CASE("beam2 full integration locks while beam3 does not", "[beam3][comparison]") {
	// 把两个单元的完全积分为放在同一个细长比下直接对比。
	// 这是"1 节点单元 + 完全积分"这一列的答案：
	//     2 节点 → 锁死（挠度趋于 0）
	//     3 节点 → 不锁死（挠度趋于解析解的 3/4）
	const double P = 1000.0;
	const double L = kLength;
	const double hOverL = 0.001;
	const double h = hOverL * L;

	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(h);

	const double EI  = bendingStiffness(section);
	const double kGA = shearStiffness(material, section);
	const double wExact = P * L * L * L / (3.0 * EI) + P * L / kGA;

	// 3 节点
	const feml::Mesh mesh3 = makeMesh3(1, L);
	const TipResponse tip3 = solveCantilever3(
		mesh3, material, section, feml::IntegrationScheme::Full, P, 0.0);
	const double ratio3 = tip3.w / wExact;

	// 2 节点
	feml::Mesh mesh2;
	mesh2.node_coords.resize(2, 1);
	mesh2.node_coords << 0.0, L;
	mesh2.element_type.assign(1, "BEAM2");
	mesh2.element_nodes.assign(1, std::vector<feml::NodeId>{ 0, 1 });

	feml::DofManager dofs2(2, 2);
	dofs2.constrain(0, 0);
	dofs2.constrain(0, 1);
	dofs2.number();

	feml::StaticProblem problem2;
	problem2.mesh     = &mesh2;
	problem2.material = &material;
	problem2.section  = &section;
	problem2.dofs     = &dofs2;
	problem2.scheme   = feml::IntegrationScheme::Full;
	problem2.nodalLoads = Eigen::VectorXd::Zero(4);
	problem2.nodalLoads(2) = P;

	feml::EigenLdltSolver solver;
	const feml::StaticResult result2 = feml::solveStatic(problem2, solver);
	const double ratio2 = result2.displacement(2) / wExact;

	INFO("h/L = " << hOverL);
	INFO("BEAM2 full integration ratio = " << ratio2);
	INFO("BEAM3 full integration ratio = " << ratio3);

	REQUIRE(ratio2 < 0.01);        // 2 节点：几乎压死
	REQUIRE(ratio3 > 0.7);         // 3 节点：仍然能弯

	// 两者的量级差距应当非常显著
	REQUIRE(ratio3 / ratio2 > 100.0);
}

TEST_CASE("beam3 reduced integration does not lock", "[beam3][locking]") {
	const double P = 1000.0;
	const double L = kLength;

	for (const feml::IntegrationScheme scheme : {
	         feml::IntegrationScheme::Reduced,
	         feml::IntegrationScheme::SelectiveReduced }) {

		for (const double hOverL : { 0.2, 0.05, 0.01, 0.001 }) {
			const double h = hOverL * L;

			const feml::Material material = makeMaterial();
			const feml::Section section = makeSection(h);

			const feml::Mesh mesh = makeMesh3(1, L);
			const TipResponse tip =
				solveCantilever3(mesh, material, section, scheme, P, 0.0);

			const double EI  = bendingStiffness(section);
			const double kGA = shearStiffness(material, section);
			const double wExact = P * L * L * L / (3.0 * EI) + P * L / kGA;

			const double ratio = tip.w / wExact;

			INFO("scheme = " << static_cast<int>(scheme)
			                 << ", h/L = " << hOverL << ", ratio = " << ratio);

			// 不锁死：挠度比有下界
			REQUIRE(ratio > 0.8);
			REQUIRE(ratio < 1.5);

			// 【更强的结论】3 节点单元用减缩积分时，
			// 【1 个单元就给出精确的端部挠度】—— 与细长比和单元数无关。
			//
			// 原因是 3 节点单元比 2 节点多一个"存活变形模式"：
			// 剪切项减缩到 2 点后，二次的 γ 只受 2 个约束，第 3 个系数自由，
			// 于是插值空间里保留了一个能精确匹配解析解弯曲模式的子空间。
			REQUIRE_THAT(ratio, WithinRel(1.0, 1e-6));
		}
	}
}

// ===========================================================================
//  与 2 节点单元的比较（作业要求"并比较"）
// ===========================================================================

TEST_CASE("beam3 converges faster than beam2 under tip load", "[beam3][comparison]") {
	// 作业要求"重复以上算例，并比较"。
	// 最直接的比较就是：同样自由度下谁更准。
	//
	// 注意这是【自由度】层面的公平比较，不是单元数层面：
	//   1 个 BEAM3（3 节点、6 自由度）= 2 个 BEAM2（各 4 自由度，
	//   但共享中间节点，合计 3 节点 6 自由度）
	const double P = 1000.0;
	const double L = kLength;

	const feml::Material material = makeMaterial();
	const feml::Section section = makeSection(kHeight);

	const double EI  = bendingStiffness(section);
	const double kGA = shearStiffness(material, section);
	const double wExact = P * L * L * L / (3.0 * EI) + P * L / kGA;

	// 3 节点单元：1 个单元 = 6 个自由度
	const feml::Mesh mesh3 = makeMesh3(1, L);
	const TipResponse tip3 = solveCantilever3(
		mesh3, material, section, feml::IntegrationScheme::Reduced, P, 0.0);

	// 2 节点单元：2 个单元 = 3 节点 = 6 个自由度
	const auto solveBeam2 = [&](int elementCount) -> double {
		feml::Mesh mesh;
		const int nodeCount = elementCount + 1;
		mesh.node_coords.resize(nodeCount, 1);
		for (int i = 0; i < nodeCount; ++i) {
			mesh.node_coords(i, 0) = L * i / (nodeCount - 1);
		}
		mesh.element_type.assign(static_cast<std::size_t>(elementCount), "BEAM2");
		mesh.element_nodes.resize(static_cast<std::size_t>(elementCount));
		for (int e = 0; e < elementCount; ++e) {
			mesh.element_nodes[static_cast<std::size_t>(e)] = std::vector<feml::NodeId>{
				static_cast<feml::NodeId>(e), static_cast<feml::NodeId>(e + 1)
			};
		}

		feml::DofManager dofs(mesh.nodeCount(), 2);
		dofs.constrain(0, 0);
		dofs.constrain(0, 1);
		dofs.number();

		feml::StaticProblem problem;
		problem.mesh     = &mesh;
		problem.material = &material;
		problem.section  = &section;
		problem.dofs     = &dofs;
		problem.scheme   = feml::IntegrationScheme::Reduced;
		problem.nodalLoads = Eigen::VectorXd::Zero(2 * mesh.nodeCount());
		problem.nodalLoads((nodeCount - 1) * 2) = P;

		feml::EigenLdltSolver solver;
		const feml::StaticResult result = feml::solveStatic(problem, solver);

		return std::abs(result.displacement((nodeCount - 1) * 2) - wExact) / wExact;
	};

	const double errorBeam2TwoElements = solveBeam2(2);

	INFO("BEAM3 1 element (6 dof) error  = " << std::abs(tip3.w - wExact) / wExact);
	INFO("BEAM2 2 elements (6 dof) error = " << errorBeam2TwoElements);

	// 同等自由度下 3 节点单元应该更准
	REQUIRE(std::abs(tip3.w - wExact) / wExact < errorBeam2TwoElements);
}
