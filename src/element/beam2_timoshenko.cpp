#include "feml/element/beam2_timoshenko.hpp"

#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <Eigen/Core>

#include <cmath>

namespace feml {
namespace {

/// 弯曲项需要的高斯点数。

int bendingOrder(IntegrationScheme scheme) {
	switch (scheme) {
	case IntegrationScheme::Full:             return 2;
	case IntegrationScheme::Reduced:          return 1;
	case IntegrationScheme::SelectiveReduced: return 2;
	}
	return 2;  
}

/// 剪切项需要的高斯点数。

int shearOrder(IntegrationScheme scheme) {
	switch (scheme) {
	case IntegrationScheme::Full:             return 2;
	case IntegrationScheme::Reduced:          return 1;
	case IntegrationScheme::SelectiveReduced: return 1;
	}
	return 2;
}

/// 弯曲项的 B 矩阵（1×4）

Eigen::Matrix<double, 1, 4> bendingB(double L) {
	Eigen::Matrix<double, 1, 4> B;
	B << 0.0, -1.0 / L, 0.0, 1.0 / L;
	return B;
}

/// 剪切项的 B 矩阵（1×4）

Eigen::Matrix<double, 1, 4> shearB(double xi, double L) {
	const double N1 = (1.0 - xi) / 2.0;
	const double N2 = (1.0 + xi) / 2.0;

	Eigen::Matrix<double, 1, 4> B;
	B << -1.0 / L, -N1, 1.0 / L, -N2;
	return B;
}

} // namespace

/// 刚度阵：
/// Eigen::MatrixXd    =  Matrix<double, Dynamic, Dynamic>     X=Dynamic, d=double
void Beam2Timoshenko::stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const {
	// ---- 1. 几何 ----
	const double x1 = ctx.node_coords(0, 0);
	const double x2 = ctx.node_coords(1, 0);
	const double L = std::abs(x2 - x1);

	// ---- 2. 材料与截面 ----
	const double E = ctx.material->E;
	const double G = ctx.material->shearModulus();

	const double A  = ctx.section->area;
	const double I  = ctx.section->inertia;
	const double ks = ctx.section->shearFactor;

	const double EI   = E * I;
	const double ksGA = ks * G * A;

	// ---- 3. 积分 ----
	const double jacobian = L / 2.0;   // dx/dξ

	Ke = Eigen::MatrixXd::Zero(4, 4);

	// 弯曲项
	{
		const Eigen::Matrix<double, 1, 4> B = bendingB(L);
		const Eigen::Matrix<double, 4, 4> BtB = B.transpose() * B;

		const GaussRule1D rule = GaussRule1D::make(bendingOrder(ctx.scheme));
		for (const GaussPoint1D& gp : rule.points()) {
			Ke += BtB * (EI * gp.weight * jacobian);
		}
	}

	// 剪切项
	{
		const GaussRule1D rule = GaussRule1D::make(shearOrder(ctx.scheme));
		for (const GaussPoint1D& gp : rule.points()) {
			const Eigen::Matrix<double, 1, 4> B = shearB(gp.xi, L);
			Ke += (B.transpose() * B) * (ksGA * gp.weight * jacobian);
		}
	}
}

} // namespace feml
