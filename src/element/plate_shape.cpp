#include "feml/element/plate_shape.hpp"

namespace feml {
namespace {

//  一维二次 Lagrange 基函数及其导数

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
// ---------------------------------------------------------------------------

PlateShapeValues shape9Lagrange(Real xi, Real eta) {
	PlateShapeValues s;
	s.nodeCount = 9;

	const std::array<Real, 9>& x = lagrange9Xi();
	const std::array<Real, 9>& y = lagrange9Eta();

	for (std::size_t i = 0; i < x.size(); ++i) {
		const int a = static_cast<int>(x[i]);
		const int b = static_cast<int>(y[i]);

		// 坐标为 -1 / 0 / +1，直接作为一维基函数的编号
		s.N[i]      = lagrange1D(a, xi) * lagrange1D(b, eta);
		s.dNdXi[i]  = lagrange1Dderiv(a, xi) * lagrange1D(b, eta);
		s.dNdEta[i] = lagrange1D(a, xi) * lagrange1Dderiv(b, eta);
	}

	return s;
}

// ---------------------------------------------------------------------------
//  8 节点 Serendipity
// ---------------------------------------------------------------------------

PlateShapeValues shape8Serendipity(Real xi, Real eta) {
	PlateShapeValues s;
	s.nodeCount = 8;

	const std::array<Real, 8>& x = serendipity8Xi();
	const std::array<Real, 8>& y = serendipity8Eta();

	for (std::size_t k = 0; k < x.size(); ++k) {
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
