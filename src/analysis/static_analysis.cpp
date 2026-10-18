#include "feml/analysis/static_analysis.hpp"

#include "feml/analysis/assembler.hpp"
#include "feml/core/exceptions.hpp"
#include "feml/linalg/solver.hpp"
#include "feml/model/dof_manager.hpp"
#include "feml/model/material.hpp"
#include "feml/model/section.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace feml {
namespace {

/// 确认所有指定位移都是零。
void requireZeroPrescribed(const DofManager& dofs) {
	for (int node = 0; node < dofs.nodeCount(); ++node) {
		for (int comp = 0; comp < dofs.dofPerNode(); ++comp) {
			const NodeId n = static_cast<NodeId>(node);
			const DofComponent c = static_cast<DofComponent>(comp);
			if (dofs.isConstrained(n, c) && dofs.prescribedValue(n, c) != 0.0) {
				throw InputError(
					"暂不支持非零指定位移（节点 " + std::to_string(node)
					+ "、分量 " + std::to_string(comp) + " 的指定值为 "
					+ std::to_string(dofs.prescribedValue(n, c)) + "）");
			}
		}
	}
}

} // namespace

StaticResult solveStatic(const StaticProblem& problem, LinearSolver& solver) {
	// ---- 0. 输入校验 ----
	if (problem.mesh == nullptr || problem.material == nullptr
	    || problem.section == nullptr || problem.dofs == nullptr) {
		throw InputError("StaticProblem 里有未设置的指针（mesh / material / section / dofs）");
	}

	const Mesh&       mesh = *problem.mesh;
	const DofManager& dofs = *problem.dofs;

	const int total = dofs.totalDof();
	if (problem.nodalLoads.size() != total) {
		throw InputError("载荷向量长度 " + std::to_string(problem.nodalLoads.size())
		                 + " 与总自由度数 " + std::to_string(total) + " 不一致");
	}

	requireZeroPrescribed(dofs);

	// ---- 1. 组装总刚 ----
	const Eigen::MatrixXd K = assembleStiffness(mesh, *problem.material,
	                                            *problem.section, problem.scheme);

	// ---- 2. 建立「方程号 → 全局自由度下标」的映射 ----
	const int neq = dofs.equationCount();
	const int ndof = dofs.dofPerNode();

	std::vector<int> equationToGlobal(static_cast<std::size_t>(neq));
	for (int e = 0; e < neq; ++e) {
		const int node = static_cast<int>(dofs.nodeOf(static_cast<Index>(e)));
		const int comp = static_cast<int>(dofs.componentOf(static_cast<Index>(e)));
		equationToGlobal[static_cast<std::size_t>(e)] = node * ndof + comp;
	}

	// ---- 3. 添加约束：消元法
	Eigen::MatrixXd Kff(neq, neq);
	Eigen::VectorXd Ff(neq);

	for (int a = 0; a < neq; ++a) {
		const int ia = equationToGlobal[static_cast<std::size_t>(a)];
		Ff(a) = problem.nodalLoads(ia);
		for (int b = 0; b < neq; ++b) {
			const int ib = equationToGlobal[static_cast<std::size_t>(b)];
			Kff(a, b) = K(ia, ib);
		}
	}

	// ---- 4. 求解 ----
	Eigen::VectorXd uf;
	if (!solver.solve(Kff, Ff, uf)) {
		throw SolverError("线性方程组求解失败（求解器 " + std::string(solver.name())
		                  + "）。");
	}

	// ---- 5. 组装完整的位移向量 ----
	StaticResult result;
	result.displacement = Eigen::VectorXd::Zero(total);
	result.solverName   = std::string(solver.name());

	// 先填被约束自由度（指定位移值）
	for (int node = 0; node < dofs.nodeCount(); ++node) {
		for (int comp = 0; comp < ndof; ++comp) {
			const NodeId n = static_cast<NodeId>(node);
			const DofComponent c = static_cast<DofComponent>(comp);
			if (dofs.isConstrained(n, c)) {
				result.displacement(node * ndof + comp) = dofs.prescribedValue(n, c);
			}
		}
	}

	// 再填自由自由度
	for (int e = 0; e < neq; ++e) {
		result.displacement(equationToGlobal[static_cast<std::size_t>(e)]) = uf(e);
	}

	// ---- 6. 支反力 R = K·u − F ----
	result.reaction = K * result.displacement - problem.nodalLoads;

	return result;
}

} // namespace feml
