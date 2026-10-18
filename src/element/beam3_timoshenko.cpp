#include "feml/element/beam3_timoshenko.hpp"

#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <Eigen/Core>

#include <array>
#include <cmath>

namespace feml {
namespace {

/// 每个节点 2 个自由度，3 个节点共 6 个
constexpr int kDof = 6;

/// ξ 处的形函数值、形函数导数、以及 Jacobian
struct ShapeAt {
	std::array<double, 3> N{};       ///< 形函数值
	std::array<double, 3> dNdXi{};   ///< 形函数对 ξ 的导数
	double J = 0.0;                  ///< J = dx/dξ
};

/// 二次 Lagrange 形函数：
///     N₁ = ξ(ξ-1)/2     N₂ = 1-ξ²     N₃ = ξ(ξ+1)/2 
/// 对应自然坐标 ξ = -1, 0, +1。
ShapeAt shapeAt(double xi, const std::array<double, 3>& x) {
	ShapeAt s;

	s.N[0] = 0.5 * xi * (xi - 1.0);
	s.N[1] = 1.0 - xi * xi;
	s.N[2] = 0.5 * xi * (xi + 1.0);

	s.dNdXi[0] = xi - 0.5;
	s.dNdXi[1] = -2.0 * xi;
	s.dNdXi[2] = xi + 0.5;

	// J = dx/dξ。等距节点时它等于 L/2 且与 ξ 无关。
	s.J = s.dNdXi[0] * x[0] + s.dNdXi[1] * x[1] + s.dNdXi[2] * x[2];

	return s;
}

/// 弯曲项需要的高斯点数。
int bendingOrder(IntegrationScheme scheme) {
	switch (scheme) {
	case IntegrationScheme::Full:             return 3;
	case IntegrationScheme::Reduced:          return 2;
	case IntegrationScheme::SelectiveReduced: return 3;
	}
	return 3;   // 理论上不可达，用于消除"并非所有路径都有返回值"的警告
}

/// 剪切项需要的高斯点数。
int shearOrder(IntegrationScheme scheme) {
	switch (scheme) {
	case IntegrationScheme::Full:             return 3;
	case IntegrationScheme::Reduced:          return 2;
	case IntegrationScheme::SelectiveReduced: return 2;
	}
	return 3;
}

/// 弯曲项的 B 矩阵
Eigen::Matrix<double, 1, kDof> bendingB(const ShapeAt& s) {
	const double invJ = 1.0 / s.J;

	Eigen::Matrix<double, 1, kDof> B;
	B << 0.0,            invJ * s.dNdXi[0],
	     0.0,            invJ * s.dNdXi[1],
	     0.0,            invJ * s.dNdXi[2];
	return B;
}

/// 剪切项的 B 矩阵
Eigen::Matrix<double, 1, kDof> shearB(const ShapeAt& s) {
	const double invJ = 1.0 / s.J;

	Eigen::Matrix<double, 1, kDof> B;
	B << invJ * s.dNdXi[0], -s.N[0],
	     invJ * s.dNdXi[1], -s.N[1],
	     invJ * s.dNdXi[2], -s.N[2];
	return B;
}

} // namespace

/// 刚度阵：
void Beam3Timoshenko::stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const {
	// ---- 1. 几何 ----
	std::array<double, 3> x{};
	for (int k = 0; k < 3; ++k) {
		x[static_cast<std::size_t>(k)] = ctx.node_coords(k, 0);
	}

	// ---- 2. 材料与截面 ----
	const double G = ctx.material->shearModulus();

	const double EI   = ctx.material->E * ctx.section->inertia;
	const double ksGA = ctx.section->shearFactor * G * ctx.section->area;

	// ---- 3. 积分 ----
	Ke = Eigen::MatrixXd::Zero(kDof, kDof);

	// 弯曲项
	{
		const GaussRule1D rule = GaussRule1D::make(bendingOrder(ctx.scheme));
		for (const GaussPoint1D& gp : rule.points()) {
			const ShapeAt s = shapeAt(gp.xi, x);
			const Eigen::Matrix<double, 1, kDof> B = bendingB(s);
			Ke += (B.transpose() * B) * (EI * gp.weight * s.J);
		}
	}

	// 剪切项
	{
		const GaussRule1D rule = GaussRule1D::make(shearOrder(ctx.scheme));
		for (const GaussPoint1D& gp : rule.points()) {
			const ShapeAt s = shapeAt(gp.xi, x);
			const Eigen::Matrix<double, 1, kDof> B = shearB(s);
			Ke += (B.transpose() * B) * (ksGA * gp.weight * s.J);
		}
	}
}

} // namespace feml
