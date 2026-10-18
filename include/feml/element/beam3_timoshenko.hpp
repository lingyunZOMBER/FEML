#pragma once

#include "feml/element/element.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace feml {

/// 3 节点二次 Timoshenko 梁单元。
///
/// 自由度：w（挠度）、θ（截面转角），每节点 2 个，共 6 个：
///     u_e = [ w1, theta1, w2, theta2, w3, theta3 ]T
/// 
/// 积分方案到积分点数的映射：
///     方案              弯曲项   剪切项
///     Full               3 点     3 点    
///     Reduced            2 点     2 点
///     SelectiveReduced   3 点     2 点
class Beam3Timoshenko final : public Element {
public:
	std::string_view name() const override { return "BEAM3"; }
	int nodeCount() const override { return 3; }
	int dofPerNode() const override { return 2; }
	int spatialDim() const override { return 1; }
	std::vector<std::string> dofNames() const override { return { "W", "THETA" }; }

	/// 计算单元刚度矩阵。积分方案由 ctx.scheme 决定。
	void stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const override;
};

} // namespace feml
