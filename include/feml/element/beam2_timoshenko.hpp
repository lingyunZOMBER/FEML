#pragma once

#include "feml/element/element.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace feml {

	/// 2 节点 Timoshenko 梁单元。
	///
	/// 自由度：w（挠度）、θ（截面转角），每节点 2 个，共 4 个：
	///     u_e = [ w₁, θ₁, w₂, θ₂ ]ᵀ
	/// 
	/// 积分方案到积分点数的映射：
	///     方案              弯曲项   剪切项
	///     Full               2 点     2 点    
	///     Reduced            1 点     1 点    
	///     SelectiveReduced   2 点     1 点    
	class Beam2Timoshenko final : public Element {
	public:
		std::string_view name() const override { return "BEAM2"; }
		int nodeCount() const override { return 2; }
		int dofPerNode() const override { return 2; }
		int spatialDim() const override { return 1; }
		std::vector<std::string> dofNames() const override { return { "W", "THETA" }; }

		/// 计算单元刚度矩阵。积分方案由 ctx.scheme 决定。
		void stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const override;
	};

} // namespace feml