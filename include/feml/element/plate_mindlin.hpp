#pragma once

#include "feml/element/element.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace feml {

// ---------------------------------------------------------------------------
//  Mindlin 板单元
//
//  每节点 3 个自由度：w（挠度）、θx、θy（转角）
//
//  广义应变：
//      弯曲  κ = [ ∂θx/∂x,  ∂θy/∂y,  ∂θx/∂y + ∂θy/∂x ]
//      剪切  γ = [ ∂w/∂x − θx,  ∂w/∂y − θy ]
//
//  本构：
//      弯曲  D_b = D₀·[[1, ν, 0], [ν, 1, 0], [0, 0, (1−ν)/2]]
//            D₀ = E·t³ / (12(1−ν²))
//      剪切  D_s = k_s·G·t·I₂
//
//  积分方案（两种单元相同）：
//      Full              弯曲 3×3，剪切 3×3
//      Reduced           弯曲 2×2，剪切 2×2
//      SelectiveReduced  弯曲 3×3，剪切 2×2
// ---------------------------------------------------------------------------

/// 9 节点 Lagrange Mindlin 板单元
class Plate9Lagrange final : public Element {
public:
	std::string_view name() const override { return "PLATE9"; }
	int nodeCount() const override { return 9; }
	int dofPerNode() const override { return 3; }
	int spatialDim() const override { return 2; }

	std::vector<std::string> dofNames() const override {
		return { "W", "THETA_X", "THETA_Y" };
	}

	/// 单元刚度矩阵（尺寸 27×27）
	void stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const override;

	/// 均布横向载荷 q 的一致节点载荷向量（尺寸 27）
	void loadVector(const ElementContext& ctx, Eigen::VectorXd& Fe) const override;
};

/// 8 节点 Serendipity Mindlin 板单元
class Plate8Serendipity final : public Element {
public:
	std::string_view name() const override { return "PLATE8"; }
	int nodeCount() const override { return 8; }
	int dofPerNode() const override { return 3; }
	int spatialDim() const override { return 2; }

	std::vector<std::string> dofNames() const override {
		return { "W", "THETA_X", "THETA_Y" };
	}

	/// 单元刚度矩阵（尺寸 24×24）
	void stiffness(const ElementContext& ctx, Eigen::MatrixXd& Ke) const override;

	/// 均布横向载荷 q 的一致节点载荷向量（尺寸 24）
	void loadVector(const ElementContext& ctx, Eigen::VectorXd& Fe) const override;
};

} // namespace feml
