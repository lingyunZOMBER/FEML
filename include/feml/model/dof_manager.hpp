#pragma once

#include "feml/core/types.hpp"

#include <cstddef>
#include <vector>

namespace feml {

/// 自由度管理器：把「节点 + 分量」映射到全局方程号。
///
/// 【使用顺序】
///     1. 构造（指定节点数和每节点自由度数）
///     2. constrain(...) 施加位移边界条件，可多次
///     3. number() 分配方程号
///     4. equationNumber(...) 查询
class DofManager {
public:
	DofManager(int nodeCount, int dofPerNode);

	int nodeCount()  const { return node_count_; }
	int dofPerNode() const { return dof_per_node_; }
	int totalDof()   const { return node_count_ * dof_per_node_; }

	// ---- 约束 ----------------------------------------------------------

	/// 把某个自由度设为指定位移（默认 0，即固支）
	/// @throws InputError 节点或分量越界
	/// @throws SolverError 已经调用过 number()（改约束必须重新编号）
	void constrain(NodeId node, DofComponent comp, Real value = 0.0);

	bool isConstrained(NodeId node, DofComponent comp) const;
	Real prescribedValue(NodeId node, DofComponent comp) const;

	// ---- 编号 ----------------------------------------------------------

	/// 分配方程号：未受约束的自由度依次编号 0,1,2,...；受约束的为 KInvalidIndex。
	/// @throws SolverError 全部自由度都被约束（没有任何方程可解）
	void number();

	bool isNumbered() const { return numbered_; }

	/// @return 方程号；KInvalidIndex 表示该自由度被约束
	/// @throws SolverError 尚未调用 number()
	Index equationNumber(NodeId node, DofComponent comp) const;

	/// 自由度数（方程个数）
	int equationCount() const { return equation_count_; }

	/// 反查：方程号 → 节点
	NodeId nodeOf(Index eq) const {
		return eq_node_[static_cast<std::size_t>(eq)];
	}

	/// 反查：方程号 → 分量
	DofComponent componentOf(Index eq) const {
		return eq_comp_[static_cast<std::size_t>(eq)];
	}

private:
	/// 把 (节点, 分量) 展平成一维下标，并做越界检查
	std::size_t flat(NodeId node, DofComponent comp) const;

	int node_count_;
	int dof_per_node_;

	std::vector<bool>  constrained_;   ///< (node,comp) → 是否被约束
	std::vector<Real>  prescribed_;    ///< (node,comp) → 指定位移值
	std::vector<Index> equation_;      ///< (node,comp) → 方程号

	std::vector<NodeId>       eq_node_;   ///< 方程号 → 节点
	std::vector<DofComponent> eq_comp_;   ///< 方程号 → 分量

	int  equation_count_ = 0;
	bool numbered_ = false;
};

} // namespace feml
