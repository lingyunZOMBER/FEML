#include "feml/model/dof_manager.hpp"

#include "feml/core/exceptions.hpp"

#include <string>

namespace feml {

DofManager::DofManager(int nodeCount, int dofPerNode)
	: node_count_(nodeCount), dof_per_node_(dofPerNode)
{
	if (nodeCount <= 0) {
		throw InputError("节点数必须为正，收到 " + std::to_string(nodeCount));
	}
	if (dofPerNode <= 0) {
		throw InputError("每节点自由度数必须为正，收到 " + std::to_string(dofPerNode));
	}

	const std::size_t n = static_cast<std::size_t>(node_count_)
	                    * static_cast<std::size_t>(dof_per_node_);

	constrained_.assign(n, false);
	prescribed_.assign(n, 0.0);
	equation_.assign(n, KInvalidIndex);
}

std::size_t DofManager::flat(NodeId node, DofComponent comp) const {
	if (node < 0 || node >= node_count_) {
		throw InputError("节点编号 " + std::to_string(node) + " 越界");
	}
	if (comp < 0 || comp >= dof_per_node_) {
		throw InputError("自由度分量 " + std::to_string(comp)
		                 + " 超出范围 [0, " + std::to_string(dof_per_node_ - 1) + "]");
	}
	return static_cast<std::size_t>(node) * static_cast<std::size_t>(dof_per_node_)
	     + static_cast<std::size_t>(comp);
}

void DofManager::constrain(NodeId node, DofComponent comp, Real value) {
	if (numbered_) {
		throw SolverError("编号之后再施加约束会破坏方程号，必须先 constrain 再 number");
	}

	const std::size_t i = flat(node, comp);
	constrained_[i] = true;
	prescribed_[i]  = value;
}

bool DofManager::isConstrained(NodeId node, DofComponent comp) const {
	return constrained_[flat(node, comp)];
}

Real DofManager::prescribedValue(NodeId node, DofComponent comp) const {
	return prescribed_[flat(node, comp)];
}

void DofManager::number() {
	// 允许重复调用（重新编号），但必须先把旧结果清干净
	equation_.assign(equation_.size(), KInvalidIndex);
	eq_node_.clear();
	eq_comp_.clear();
	equation_count_ = 0;

	// 按"节点优先、分量其次"的顺序编号。

	for (int node = 0; node < node_count_; ++node) {
		for (int comp = 0; comp < dof_per_node_; ++comp) {
			const std::size_t i = static_cast<std::size_t>(node)
			                    * static_cast<std::size_t>(dof_per_node_)
			                    + static_cast<std::size_t>(comp);
			if (constrained_[i]) {
				continue;
			}
			equation_[i] = equation_count_;
			eq_node_.push_back(static_cast<NodeId>(node));
			eq_comp_.push_back(static_cast<DofComponent>(comp));
			++equation_count_;
		}
	}

	if (equation_count_ == 0) {
		throw SolverError("所有自由度都被约束，没有方程可解");
	}

	numbered_ = true;
}

Index DofManager::equationNumber(NodeId node, DofComponent comp) const {
	if (!numbered_) {
		throw SolverError("尚未调用 number() 分配方程号");
	}
	return equation_[flat(node, comp)];
}

} // namespace feml
