#pragma once

#include "feml/core/types.hpp"

#include <string_view>
#include <vector>

namespace feml {

// ---------------------------------------------------------------------------
//  一维高斯积分
// ---------------------------------------------------------------------------

/// 一维高斯积分点：ξ ∈ [-1, 1]
struct GaussPoint1D {
	Real xi     = 0.0;   ///< 积分点坐标
	Real weight = 0.0;   ///< 权重
};

/// 一维高斯-勒让德积分规则
class GaussRule1D {
public:
	/// 构造 n 点规则，n ∈ [1, 4]
	/// @throws InputError n 超出范围
	static GaussRule1D make(int order);

	int order() const { return static_cast<int>(points_.size()); }
	const std::vector<GaussPoint1D>& points() const { return points_; }

private:
	std::vector<GaussPoint1D> points_;
};

// ---------------------------------------------------------------------------
//  二维高斯积分（两个一维规则的张量积）
// ---------------------------------------------------------------------------

/// 二维高斯积分点：(ξ, η) ∈ [-1,1]²
struct GaussPoint2D {
	Real xi     = 0.0;   ///< 第一方向坐标
	Real eta    = 0.0;   ///< 第二方向坐标
	Real weight = 0.0;   ///< 权重（两个一维权重的乘积）
};

/// 二维高斯-勒让德积分规则。
///
/// 由 nx 点与 ny 点的一维规则张量积构成，共 nx*ny 个积分点。
/// 对每个方向独立精确到 2n-1 次。
class GaussRule2D {
public:
	/// 构造 nx × ny 点规则，两个参数都在 [1, 4]
	/// @throws InputError 参数超出范围
	static GaussRule2D make(int nx, int ny);

	int count() const { return static_cast<int>(points_.size()); }
	const std::vector<GaussPoint2D>& points() const { return points_; }

private:
	std::vector<GaussPoint2D> points_;
};

// ---------------------------------------------------------------------------
//  积分方案
// ---------------------------------------------------------------------------

/// 积分方案
enum class IntegrationScheme {
	Full,              ///< 完全积分
	Reduced,           ///< 减缩积分
	SelectiveReduced,  ///< 选择性减缩（只对剪切项降阶）
};

/// 从关键字解析积分方案
/// @throws InputError 关键字无法识别
IntegrationScheme parseIntegrationScheme(std::string_view text);

} // namespace feml
