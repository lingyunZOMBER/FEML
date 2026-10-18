#include "feml/element/plate_mindlin.hpp"
#include "feml/element/plate_shape.hpp"
#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <Eigen/Core>
#include <Eigen/Eigenvalues>

#include <cmath>
#include <cstddef>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace {

constexpr double kE      = 200.0e9;   ///< 弹性模量 [Pa]
constexpr double kNu     = 0.3;       ///< 泊松比
constexpr double kA      = 1.0;       ///< 板边长 [m]
constexpr double kT      = 0.05;      ///< 板厚 [m]
constexpr double kShear = 5.0 / 6.0;  ///< 剪切修正因子

/// 生成 a × a 的方形板单元节点坐标（按自然坐标表映射）
template <std::size_t N>
Eigen::MatrixXd makeSquareCoords(const std::array<double, N>& xi,
                                 const std::array<double, N>& eta) {
	Eigen::MatrixXd coords(static_cast<Eigen::Index>(N), 2);

	for (std::size_t i = 0; i < N; ++i) {
		coords(static_cast<Eigen::Index>(i), 0) = 0.5 * kA * (1.0 + xi[i]);
		coords(static_cast<Eigen::Index>(i), 1) = 0.5 * kA * (1.0 + eta[i]);
	}

	return coords;
}

feml::Material makeMaterial() {
	feml::Material m;
	m.id = 1;
	m.E  = kE;
	m.nu = kNu;
	return m;
}

feml::Section makeSection() {
	feml::Section s;
	s.id          = 1;
	s.thickness   = kT;
	s.shearFactor = kShear;
	return s;
}

feml::ElementContext makeContext(const Eigen::MatrixXd& coords,
                                 feml::IntegrationScheme scheme) {
	static feml::Material material = makeMaterial();
	static feml::Section  section  = makeSection();

	feml::ElementContext ctx;
	ctx.node_coords = coords;
	ctx.material    = &material;
	ctx.section     = &section;
	ctx.scheme      = scheme;
	return ctx;
}

/// 数出接近零的特征值个数
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

/// 构造三个刚体模态：平移、绕 y 轴转动、绕 x 轴转动
///
///   w = a + b·x + c·y,   θx = b,  θy = c
template <std::size_t N>
std::vector<Eigen::VectorXd> rigidBodyModes(const Eigen::MatrixXd& coords) {
	std::vector<Eigen::VectorXd> modes;

	// 平移：w = 1
	Eigen::VectorXd t = Eigen::VectorXd::Zero(3 * N);
	for (std::size_t i = 0; i < N; ++i) {
		t(static_cast<Eigen::Index>(3 * i + 0)) = 1.0;
	}
	modes.push_back(t);

	// 绕 y 轴转动：w = x，θx = 1
	Eigen::VectorXd ry = Eigen::VectorXd::Zero(3 * N);
	for (std::size_t i = 0; i < N; ++i) {
		ry(static_cast<Eigen::Index>(3 * i + 0)) = coords(static_cast<Eigen::Index>(i), 0);
		ry(static_cast<Eigen::Index>(3 * i + 1)) = 1.0;
	}
	modes.push_back(ry);

	// 绕 x 轴转动：w = y，θy = 1
	Eigen::VectorXd rx = Eigen::VectorXd::Zero(3 * N);
	for (std::size_t i = 0; i < N; ++i) {
		rx(static_cast<Eigen::Index>(3 * i + 0)) = coords(static_cast<Eigen::Index>(i), 1);
		rx(static_cast<Eigen::Index>(3 * i + 2)) = 1.0;
	}
	modes.push_back(rx);

	return modes;
}

constexpr feml::IntegrationScheme kAllSchemes[] = {
	feml::IntegrationScheme::Full,
	feml::IntegrationScheme::Reduced,
	feml::IntegrationScheme::SelectiveReduced,
};

} // namespace

// ===========================================================================
//  形函数
// ===========================================================================

TEST_CASE("plate shape functions sum to unity and vanish at other nodes",
          "[plate][shape]") {
	const double points[][2] = {
		{-1.0, -1.0}, {0.0, 0.0}, {1.0, 1.0}, {0.3, -0.7}
	};

	for (const auto& p : points) {
		const double xi  = p[0];
		const double eta = p[1];

		// ---- 9 节点 Lagrange ----
		{
			const feml::PlateShapeValues s = feml::shape9Lagrange(xi, eta);

			double sumN = 0.0, sumXi = 0.0, sumEta = 0.0;
			for (int i = 0; i < 9; ++i) {
				const std::size_t k = static_cast<std::size_t>(i);
				sumN   += s.N[k];
				sumXi  += s.dNdXi[k];
				sumEta += s.dNdEta[k];
			}

			INFO("Lagrange9 at (" << xi << ", " << eta << ")");
			REQUIRE_THAT(sumN,   WithinAbs(1.0, 1e-14));
			REQUIRE_THAT(sumXi,  WithinAbs(0.0, 1e-14));
			REQUIRE_THAT(sumEta, WithinAbs(0.0, 1e-14));
		}

		// ---- 8 节点 Serendipity ----
		{
			const feml::PlateShapeValues s = feml::shape8Serendipity(xi, eta);

			double sumN = 0.0, sumXi = 0.0, sumEta = 0.0;
			for (int i = 0; i < 8; ++i) {
				const std::size_t k = static_cast<std::size_t>(i);
				sumN   += s.N[k];
				sumXi  += s.dNdXi[k];
				sumEta += s.dNdEta[k];
			}

			INFO("Serendipity8 at (" << xi << ", " << eta << ")");
			REQUIRE_THAT(sumN,   WithinAbs(1.0, 1e-14));
			REQUIRE_THAT(sumXi,  WithinAbs(0.0, 1e-14));
			REQUIRE_THAT(sumEta, WithinAbs(0.0, 1e-14));
		}
	}
}

TEST_CASE("plate shape functions are interpolatory at their own nodes",
          "[plate][shape]") {
	const std::array<double, 9>& xi9  = feml::lagrange9Xi();
	const std::array<double, 9>& eta9 = feml::lagrange9Eta();

	for (int j = 0; j < 9; ++j) {
		const feml::PlateShapeValues s =
			feml::shape9Lagrange(xi9[static_cast<std::size_t>(j)],
			                     eta9[static_cast<std::size_t>(j)]);

		for (int i = 0; i < 9; ++i) {
			const double expected = (i == j) ? 1.0 : 0.0;
			REQUIRE_THAT(s.N[static_cast<std::size_t>(i)],
			             WithinAbs(expected, 1e-14));
		}
	}

	const std::array<double, 8>& xi8  = feml::serendipity8Xi();
	const std::array<double, 8>& eta8 = feml::serendipity8Eta();

	for (int j = 0; j < 8; ++j) {
		const feml::PlateShapeValues s =
			feml::shape8Serendipity(xi8[static_cast<std::size_t>(j)],
			                        eta8[static_cast<std::size_t>(j)]);

		for (int i = 0; i < 8; ++i) {
			const double expected = (i == j) ? 1.0 : 0.0;
			REQUIRE_THAT(s.N[static_cast<std::size_t>(i)],
			             WithinAbs(expected, 1e-14));
		}
	}
}

// ===========================================================================
//  单元基本信息与刚度阵性质
// ===========================================================================

TEST_CASE("plate elements report their basic information", "[plate]") {
	const feml::Plate9Lagrange p9;
	REQUIRE(p9.name() == "PLATE9");
	REQUIRE(p9.nodeCount() == 9);
	REQUIRE(p9.dofPerNode() == 3);
	REQUIRE(p9.spatialDim() == 2);
	REQUIRE(p9.totalDof() == 27);

	const feml::Plate8Serendipity p8;
	REQUIRE(p8.name() == "PLATE8");
	REQUIRE(p8.nodeCount() == 8);
	REQUIRE(p8.dofPerNode() == 3);
	REQUIRE(p8.spatialDim() == 2);
	REQUIRE(p8.totalDof() == 24);
}

TEST_CASE("plate stiffness is symmetric", "[plate]") {
	const Eigen::MatrixXd coords9 = makeSquareCoords(feml::lagrange9Xi(),
	                                                 feml::lagrange9Eta());
	const Eigen::MatrixXd coords8 = makeSquareCoords(feml::serendipity8Xi(),
	                                                 feml::serendipity8Eta());

	const feml::Plate9Lagrange p9;
	const feml::Plate8Serendipity p8;

	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		Eigen::MatrixXd K9;
		p9.stiffness(makeContext(coords9, scheme), K9);
		REQUIRE(K9.rows() == 27);
		REQUIRE((K9 - K9.transpose()).norm() < K9.norm() * 1e-14);

		Eigen::MatrixXd K8;
		p8.stiffness(makeContext(coords8, scheme), K8);
		REQUIRE(K8.rows() == 24);
		REQUIRE((K8 - K8.transpose()).norm() < K8.norm() * 1e-14);
	}
}

TEST_CASE("plate stiffness annihilates rigid body modes", "[plate]") {
	const Eigen::MatrixXd coords9 = makeSquareCoords(feml::lagrange9Xi(),
	                                                 feml::lagrange9Eta());
	const Eigen::MatrixXd coords8 = makeSquareCoords(feml::serendipity8Xi(),
	                                                 feml::serendipity8Eta());

	const feml::Plate9Lagrange p9;
	const feml::Plate8Serendipity p8;

	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		Eigen::MatrixXd K9;
		p9.stiffness(makeContext(coords9, scheme), K9);
		const double tol9 = K9.norm() * 1e-12;

		for (const Eigen::VectorXd& mode : rigidBodyModes<9>(coords9)) {
			REQUIRE((K9 * mode).norm() < tol9);
		}

		Eigen::MatrixXd K8;
		p8.stiffness(makeContext(coords8, scheme), K8);
		const double tol8 = K8.norm() * 1e-12;

		for (const Eigen::VectorXd& mode : rigidBodyModes<8>(coords8)) {
			REQUIRE((K8 * mode).norm() < tol8);
		}
	}
}

TEST_CASE("plate zero eigenvalue count reveals spurious modes", "[plate][rank]") {
	const Eigen::MatrixXd coords9 = makeSquareCoords(feml::lagrange9Xi(),
	                                                 feml::lagrange9Eta());
	const Eigen::MatrixXd coords8 = makeSquareCoords(feml::serendipity8Xi(),
	                                                 feml::serendipity8Eta());

	const feml::Plate9Lagrange p9;
	const feml::Plate8Serendipity p8;

	// 刚体模态有 3 个；零特征值多于 3 说明存在零能（hourglass）模态。
	//
	// 实测值：
	//                PLATE9   PLATE8
	//     Full          3        3
	//     Reduced       7        4
	//     Selective     4        3
	//
	// Full 两种单元都秩足够；Reduced 会引入零能模态；
	// PLATE8 配合选择性减缩既无零能模态、又避免剪切锁死。
	const int expected9[] = { 3, 7, 4 };
	const int expected8[] = { 3, 4, 3 };

	int index = 0;
	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		Eigen::MatrixXd K9;
		p9.stiffness(makeContext(coords9, scheme), K9);

		Eigen::MatrixXd K8;
		p8.stiffness(makeContext(coords8, scheme), K8);

		INFO("scheme = " << static_cast<int>(scheme));
		REQUIRE(zeroEigenvalueCount(K9) == expected9[index]);
		REQUIRE(zeroEigenvalueCount(K8) == expected8[index]);

		++index;
	}
}

// ===========================================================================
//  均布载荷向量
// ===========================================================================

TEST_CASE("uniform load vector sums to the total applied force", "[plate][load]") {
	const Eigen::MatrixXd coords9 = makeSquareCoords(feml::lagrange9Xi(),
	                                                 feml::lagrange9Eta());
	const Eigen::MatrixXd coords8 = makeSquareCoords(feml::serendipity8Xi(),
	                                                 feml::serendipity8Eta());

	const double q = 1000.0;   // N/m²
	const double expected = q * kA * kA;

	const feml::Plate9Lagrange p9;
	const feml::Plate8Serendipity p8;

	for (const feml::IntegrationScheme scheme : kAllSchemes) {
		INFO("scheme = " << static_cast<int>(scheme));

		// PLATE9：对 w 自由度求和
		feml::ElementContext ctx9 = makeContext(coords9, scheme);
		ctx9.transverse_load = q;

		Eigen::VectorXd f9;
		p9.loadVector(ctx9, f9);
		REQUIRE(f9.size() == 27);

		double sum9 = 0.0;
		for (int i = 0; i < 9; ++i) {
			sum9 += f9(3 * i);
		}
		REQUIRE_THAT(sum9, WithinAbs(expected, 1e-9 * expected));

		// PLATE8
		feml::ElementContext ctx8 = makeContext(coords8, scheme);
		ctx8.transverse_load = q;

		Eigen::VectorXd f8;
		p8.loadVector(ctx8, f8);
		REQUIRE(f8.size() == 24);

		double sum8 = 0.0;
		for (int i = 0; i < 8; ++i) {
			sum8 += f8(3 * i);
		}
		REQUIRE_THAT(sum8, WithinAbs(expected, 1e-9 * expected));
	}
}

TEST_CASE("zero load produces a zero load vector", "[plate][load]") {
	const Eigen::MatrixXd coords8 = makeSquareCoords(feml::serendipity8Xi(),
	                                                 feml::serendipity8Eta());

	feml::ElementContext ctx = makeContext(coords8, feml::IntegrationScheme::Full);
	ctx.transverse_load = 0.0;

	const feml::Plate8Serendipity p8;
	Eigen::VectorXd f;
	p8.loadVector(ctx, f);

	REQUIRE(f.size() == 24);
	REQUIRE(f.isZero());
}

// ===========================================================================
//  二维高斯积分
// ===========================================================================

TEST_CASE("two-dimensional Gauss rule integrates bilinear monomials exactly",
          "[integration][plate]") {
	// 2×2 规则对每个方向三次以内的多项式精确，因此能精确积分 ξ^a η^b（a,b ≤ 3）
	for (int nx = 1; nx <= 4; ++nx) {
		const feml::GaussRule2D rule = feml::GaussRule2D::make(nx, nx);
		REQUIRE(rule.count() == nx * nx);

		double sum = 0.0;   // ∫∫ 1 dξ dη = 4
		for (const feml::GaussPoint2D& gp : rule.points()) {
			sum += gp.weight;
		}
		REQUIRE_THAT(sum, WithinAbs(4.0, 1e-14));
	}
}
