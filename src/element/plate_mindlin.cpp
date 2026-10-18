#include "feml/element/plate_mindlin.hpp"

#include "feml/element/plate_shape.hpp"
#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <Eigen/Core>

#include <cmath>
#include <cstddef>
#include <vector>

namespace feml {
namespace {

using ShapeFn = PlateShapeValues (*)(Real, Real);

// ---------------------------------------------------------------------------
//  积分点数
// ---------------------------------------------------------------------------

/// 弯曲项需要的高斯点数（每个方向）
///
/// 弯曲项的 BᵀDB 在每个方向上二次，2 点即精确；
/// Full 取 3 点是为了统一采用能精确积分剪切项的点数。
int bendingOrder(IntegrationScheme scheme) {
	switch (scheme) {
	case IntegrationScheme::Full:             return 3;
	case IntegrationScheme::Reduced:          return 2;
	case IntegrationScheme::SelectiveReduced: return 3;
	}
	return 3;
}

/// 剪切项需要的高斯点数（每个方向）
///
/// 剪切项含形函数本身（二次），γᵀDγ 在每个方向上四次，需要 3 点才精确。
/// 减缩 / 选择减缩取 2 点，以避免剪切锁死。
int shearOrder(IntegrationScheme scheme) {
	switch (scheme) {
	case IntegrationScheme::Full:             return 3;
	case IntegrationScheme::Reduced:          return 2;
	case IntegrationScheme::SelectiveReduced: return 2;
	}
	return 3;
}

// ---------------------------------------------------------------------------
//  几何映射
// ---------------------------------------------------------------------------

/// 某积分点处的 Jacobian 与形函数的物理导数
struct Geometry {
	double dxdXi = 0.0, dydXi = 0.0;
	double dxdEta = 0.0, dydEta = 0.0;
	double detJ = 0.0;
	std::vector<double> dNdx;   ///< ∂N/∂x
	std::vector<double> dNdy;   ///< ∂N/∂y
};

/// 计算 Jacobian 与形函数的物理偏导
///
///   [∂x/∂ξ  ∂y/∂ξ]        [∂N/∂x]        [∂N/∂ξ]
///   [∂x/∂η  ∂y/∂η]  与    [∂N/∂y]   由    [∂N/∂η] 通过 J⁻¹ 联系
Geometry computeGeometry(const PlateShapeValues& s, const Eigen::MatrixXd& coords) {
	const int n = s.nodeCount;

	Geometry g;

	for (int i = 0; i < n; ++i) {
		const std::size_t k = static_cast<std::size_t>(i);
		const double x = coords(i, 0);
		const double y = coords(i, 1);

		g.dxdXi  += s.dNdXi[k]  * x;
		g.dydXi  += s.dNdXi[k]  * y;
		g.dxdEta += s.dNdEta[k] * x;
		g.dydEta += s.dNdEta[k] * y;
	}

	g.detJ = g.dxdXi * g.dydEta - g.dydXi * g.dxdEta;

	g.dNdx.assign(static_cast<std::size_t>(n), 0.0);
	g.dNdy.assign(static_cast<std::size_t>(n), 0.0);

	if (std::abs(g.detJ) < 1e-300) {
		return g;   // 退化单元：导数为零，由调用方观察到零刚度
	}

	const double invDet = 1.0 / g.detJ;

	for (int i = 0; i < n; ++i) {
		const std::size_t k = static_cast<std::size_t>(i);

		g.dNdx[k] = ( g.dydEta * s.dNdXi[k] - g.dydXi  * s.dNdEta[k]) * invDet;
		g.dNdy[k] = (-g.dxdEta * s.dNdXi[k] + g.dxdXi  * s.dNdEta[k]) * invDet;
	}

	return g;
}

// ---------------------------------------------------------------------------
//  应变矩阵
// ---------------------------------------------------------------------------

/// 弯曲应变矩阵 B_b（3 × 3n）
///
///   κx  = ∂θx/∂x
///   κy  = ∂θy/∂y
///   κxy = ∂θx/∂y + ∂θy/∂x
Eigen::MatrixXd bendingB(const Geometry& g, int n) {
	Eigen::MatrixXd B = Eigen::MatrixXd::Zero(3, 3 * n);

	for (int i = 0; i < n; ++i) {
		const std::size_t k = static_cast<std::size_t>(i);
		const int col = 3 * i;

		B(0, col + 1) = g.dNdx[k];
		B(1, col + 2) = g.dNdy[k];
		B(2, col + 1) = g.dNdy[k];
		B(2, col + 2) = g.dNdx[k];
	}

	return B;
}

/// 剪切应变矩阵 B_s（2 × 3n）
///
///   γxz = ∂w/∂x − θx
///   γyz = ∂w/∂y − θy
Eigen::MatrixXd shearB(const Geometry& g, const PlateShapeValues& s, int n) {
	Eigen::MatrixXd B = Eigen::MatrixXd::Zero(2, 3 * n);

	for (int i = 0; i < n; ++i) {
		const std::size_t k = static_cast<std::size_t>(i);
		const int col = 3 * i;

		B(0, col + 0) = g.dNdx[k];
		B(0, col + 1) = -s.N[k];

		B(1, col + 0) = g.dNdy[k];
		B(1, col + 2) = -s.N[k];
	}

	return B;
}

// ---------------------------------------------------------------------------
//  本构
// ---------------------------------------------------------------------------

/// 由材料与板厚得到弯曲、剪切刚度矩阵
void plateConstitutive(const Material& material, const Section& section,
                       Eigen::Matrix3d& Db, Eigen::Matrix2d& Ds) {
	const double E  = material.E;
	const double nu = material.nu;
	const double t  = section.thickness;
	const double G  = material.shearModulus();
	const double ks = section.shearFactor;

	const double D0 = E * t * t * t / (12.0 * (1.0 - nu * nu));

	Db.setZero();
	Db(0, 0) = D0;
	Db(0, 1) = D0 * nu;
	Db(1, 0) = D0 * nu;
	Db(1, 1) = D0;
	Db(2, 2) = D0 * (1.0 - nu) / 2.0;

	Ds.setZero();
	Ds(0, 0) = ks * G * t;
	Ds(1, 1) = ks * G * t;
}

// ---------------------------------------------------------------------------
//  刚度矩阵与载荷向量
// ---------------------------------------------------------------------------

/// 通用板单元刚度矩阵
void computePlateStiffness(ShapeFn shapeFn, int n,
                           const ElementContext& ctx, Eigen::MatrixXd& Ke) {
	Eigen::Matrix3d Db;
	Eigen::Matrix2d Ds;
	plateConstitutive(*ctx.material, *ctx.section, Db, Ds);

	const int ndof = 3 * n;
	Ke = Eigen::MatrixXd::Zero(ndof, ndof);

	// 弯曲项
	{
		const int np = bendingOrder(ctx.scheme);
		const GaussRule2D rule = GaussRule2D::make(np, np);

		for (const GaussPoint2D& gp : rule.points()) {
			const PlateShapeValues s = shapeFn(gp.xi, gp.eta);
			const Geometry g = computeGeometry(s, ctx.node_coords);
			const Eigen::MatrixXd B = bendingB(g, n);

			Ke += (B.transpose() * Db * B) * (gp.weight * std::abs(g.detJ));
		}
	}

	// 剪切项
	{
		const int np = shearOrder(ctx.scheme);
		const GaussRule2D rule = GaussRule2D::make(np, np);

		for (const GaussPoint2D& gp : rule.points()) {
			const PlateShapeValues s = shapeFn(gp.xi, gp.eta);
			const Geometry g = computeGeometry(s, ctx.node_coords);
			const Eigen::MatrixXd B = shearB(g, s, n);

			Ke += (B.transpose() * Ds * B) * (gp.weight * std::abs(g.detJ));
		}
	}
}

/// 均布横向载荷 q 的一致节点载荷向量
///
///   f_i = ∫ N_i · q · |J| dξ dη     （只作用在 w 自由度上）
void computePlateLoad(ShapeFn shapeFn, int n,
                      const ElementContext& ctx, Eigen::VectorXd& Fe) {
	const int ndof = 3 * n;
	Fe = Eigen::VectorXd::Zero(ndof);

	if (ctx.transverse_load == 0.0) {
		return;
	}

	// 载荷向量统一用 3×3 积分，与积分方案无关
	const GaussRule2D rule = GaussRule2D::make(3, 3);

	for (const GaussPoint2D& gp : rule.points()) {
		const PlateShapeValues s = shapeFn(gp.xi, gp.eta);
		const Geometry g = computeGeometry(s, ctx.node_coords);

		const double factor = gp.weight * std::abs(g.detJ) * ctx.transverse_load;

		for (int i = 0; i < n; ++i) {
			Fe(3 * i) += s.N[static_cast<std::size_t>(i)] * factor;
		}
	}
}

} // namespace

// ---------------------------------------------------------------------------
//  9 节点 Lagrange
// ---------------------------------------------------------------------------

void Plate9Lagrange::stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const {
	computePlateStiffness(&shape9Lagrange, 9, ctx, Ke);
}

void Plate9Lagrange::loadVector(const ElementContext& ctx, Eigen::VectorXd& Fe) const {
	computePlateLoad(&shape9Lagrange, 9, ctx, Fe);
}

// ---------------------------------------------------------------------------
//  8 节点 Serendipity
// ---------------------------------------------------------------------------

void Plate8Serendipity::stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const {
	computePlateStiffness(&shape8Serendipity, 8, ctx, Ke);
}

void Plate8Serendipity::loadVector(const ElementContext& ctx, Eigen::VectorXd& Fe) const {
	computePlateLoad(&shape8Serendipity, 8, ctx, Fe);
}

} // namespace feml
