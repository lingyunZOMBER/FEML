#pragma once

#include "feml/core/types.hpp"

#include <array>

namespace feml {

/// 二维形函数在自然坐标 (ξ, η) 处的取值与偏导
struct PlateShapeValues {
	std::array<Real, 9> N{};       ///< 形函数值
	std::array<Real, 9> dNdXi{};   ///< 对 ξ 的偏导
	std::array<Real, 9> dNdEta{};  ///< 对 η 的偏导
	int nodeCount = 0;             ///< 有效节点数（9 或 8）
};

// ---------------------------------------------------------------------------
//  9 节点 Lagrange 形函数
//
//  节点位置（自然坐标）：
//      4(-1, 1)   7(0, 1)   3(1, 1)
//      8(-1, 0)   9(0, 0)   6(1, 0)
//      1(-1,-1)   5(0,-1)   2(1,-1)
// ---------------------------------------------------------------------------

/// 计算 9 节点 Lagrange 形函数及其偏导
PlateShapeValues shape9Lagrange(Real xi, Real eta);

// ---------------------------------------------------------------------------
//  8 节点 Serendipity 形函数
//
//  节点位置（自然坐标）：
//      4(-1, 1)  7(0, 1)    3(1, 1)
//                
//      8(-1, 0)             6(1, 0)
//                
//      1(-1,-1)  5(0,-1)    2(1,-1)
// ---------------------------------------------------------------------------

/// 计算 8 节点 Serendipity 形函数及其偏导
PlateShapeValues shape8Serendipity(Real xi, Real eta);

// ---------------------------------------------------------------------------
//  节点自然坐标表
// ---------------------------------------------------------------------------

/// 9 节点 Lagrange 单元各节点的自然坐标（按上述编号顺序）
const std::array<Real, 9>& lagrange9Xi();
const std::array<Real, 9>& lagrange9Eta();

/// 8 节点 Serendipity 单元各节点的自然坐标（按上述编号顺序）
const std::array<Real, 8>& serendipity8Xi();
const std::array<Real, 8>& serendipity8Eta();

} // namespace feml
