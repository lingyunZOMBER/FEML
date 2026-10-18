#include "feml/element/plate_shape.hpp"

namespace feml {
namespace {

// ---------------------------------------------------------------------------
//  一维二次 Lagrange 基函数及其导数
//
//      L₋₁(t) = t(t-1)/2    L₀(t) = 1-t²    L₊₁(t) = t(t+1)/2
// ---------------------------------------------------------------------------

Real lagrange1D(int index, Real t) {
	switch (index) {
	case -1: return 0.5 * t * (t - 1.0);
	case  0: return 1.0 - t * t;
	case  1: return 0.5 * t * (t + 1.0);
	}
	return 0.0;
}

Real lagrange1Dderiv(int index, Real t) {
	switch (index) {
	case -1: return t - 0.5;
	case  0: return -2.0 * t;
	case  1: return t + 0.5;
	}
	return 0.0;
}

} // namespace

// ---------------------------------------------------------------------------
//  节点自然坐标表
// ---------------------------------------------------------------------------

const std::array<Real, 9>& lagrange9Xi() {
	static const std::array<Real, 9> xi{-1.0, 1.0, 1.0, -1.0, 0.0, 1.0, 0.0, -1.0, 0.0};
	return xi;
}

const std::array<Real, 9>& lagrange9Eta() {
	static const std::array<Real, 9> eta{-1.0, -1.0, 1.0, 1.0, -1.0, 0.0, 1.0, 0.0, 0.0};
	return eta;
}

const std::array<Real, 8>& serendipity8Xi() {
	static const std::array<Real, 8> xi{-1.0, 1.0, 1.0, -1.0, 0.0, 1.0, 0.0, -1.0};
	return xi;
}

const std::array<Real, 8>& serendipity8Eta() {
	static const std::array<Real, 8> eta{-1.0, -1.0, 1.0, 1.0, -1.0, 0.0, 1.0, 0.0};
	return eta;
}

// ---------------------------------------------------------------------------
//  9 节点 Lagrange
//
//  N_i = L_{a(i)}(ξ) · L_{b(i)}(η)，其中 a(i)、b(i) 取节点坐标对应的
//  一维基函数编号（-1、0 或 +1）。
//  节点的 a、b 编号由自然坐标表直接换算得到。
// ---------------------------------------------------------------------------

PlateShapeValues shape9Lagrange(Real xi, Real eta) {
	PlateShapeValues s;
	s.nodeCount = 9;

	const std::array<Real, 9>& x = lagrange9Xi();
	const std::array<Real, 9>& y = lagrange9Eta();

	for (int i = 0; i < 9; ++i) {
		const int a = static_cast<int>(x[static_cast<std::size_t>(i)]);
		const int b = static_cast<int>(y[static_cast<std::size_t>(i)]);

		// 坐标为 -1 / 0 / +1，直接作为一维基函数的编号
		s.N[static_cast<std::size_t>(i)] =
			lagrange1D(a, xi) * lagrange1D(b, eta);

		s.dNdXi[static_cast<std::size_t>(i)] =
			lagrange1Dderiv(a, xi) * lagrange1D(b, eta);

		s.dNdEta[static_cast<std::size_t>(i)] =
			lagrange1D(a, xi) * lagrange1Dderiv(b, eta);
	}

	return s;
}

// ---------------------------------------------------------------------------
//  8 节点 Serendipity
//
//  角节点：N = (1+ξξᵢ)(1+ηηᵢ)(ξξᵢ + ηηᵢ − 1)/4
//  边中点（ξᵢ=0）：N = (1−ξ²)(1+ηηᵢ)/2
//  边中点（ηᵢ=0）：N = (1+ξξᵢ)(1−η²)/2
// ---------------------------------------------------------------------------

PlateShapeValues shape8Serendipity(Real xi, Real eta) {
	PlateShapeValues s;
	s.nodeCount = 8;

	const std::array<Real, 8>& x = serendipity8Xi();
	const std::array<Real, 8>& y = serendipity8Eta();

	for (int i = 0; i < 8; ++i) {
		const std::size_t k = static_cast<std::size_t>(i);
		const Real xi_i  = x[k];
		const Real eta_i = y[k];

		if (xi_i != 0.0 && eta_i != 0.0) {
			// 角节点
			s.N[k] = 0.25 * (1.0 + xi * xi_i) * (1.0 + eta * eta_i)
			       * (xi * xi_i + eta * eta_i - 1.0);

			s.dNdXi[k] = 0.25 * xi_i * (1.0 + eta * eta_i)
			           * (2.0 * xi * xi_i + eta * eta_i);

			s.dNdEta[k] = 0.25 * eta_i * (1.0 + xi * xi_i)
			            * (xi * xi_i + 2.0 * eta * eta_i);
		} else if (xi_i == 0.0) {
			// ξ = 0 的边中点
			s.N[k]      = 0.5 * (1.0 - xi * xi) * (1.0 + eta * eta_i);
			s.dNdXi[k]  = -xi * (1.0 + eta * eta_i);
			s.dNdEta[k] = 0.5 * (1.0 - xi * xi) * eta_i;
		} else {
			// η = 0 的边中点
			s.N[k]      = 0.5 * (1.0 + xi * xi_i) * (1.0 - eta * eta);
			s.dNdXi[k]  = 0.5 * xi_i * (1.0 - eta * eta);
			s.dNdEta[k] = -(1.0 + xi * xi_i) * eta;
		}
	}

	return s;
}

} // namespace feml
