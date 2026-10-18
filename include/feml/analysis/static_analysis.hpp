#pragma once

#include "feml/element/integration_rule.hpp"
#include "feml/model/mesh.hpp"

#include <Eigen/Core>

#include <string>

namespace feml {

class DofManager;
class LinearSolver;
struct Material;
struct Section;

/// 一个线性静力问题的全部输入。
struct StaticProblem {
	const Mesh*       mesh     = nullptr;
	const Material*   material = nullptr;
	const Section*    section  = nullptr;
	const DofManager* dofs     = nullptr;

	IntegrationScheme scheme = IntegrationScheme::Full;

	/// 节点载荷，长度 = 节点数 × 每节点自由度数，按 (节点, 分量) 展平。
	/// 被约束的分量会被忽略（那里由指定位移接管）。
	Eigen::VectorXd nodalLoads;
};

/// 线性静力分析的结果。
struct StaticResult {
	/// 全部节点的位移，长度 = 节点数 × 每节点自由度数。
	/// 被约束的分量填的是指定位移值（现在只支持 0）。
	Eigen::VectorXd displacement;

	/// 支反力，同长度。自由自由度上理论上为 0（浮点残差量级）。
	/// 由 R = K·u − F 得到，所以包含所有自由度。
	Eigen::VectorXd reaction;

	/// 实际使用的求解器名字
	std::string solverName;
};

/// 线性静力分析：组装 → 施加约束 → 求解 → 算支反力。
///
/// 【约束施加方式：消元法】
/// 已知指定位移的自由度直接从方程组里剔除，只解剩下的自由自由度。
/// 对"固支"这类零位移约束，这等价于删掉对应的行列。
///
/// 现在只支持【零】指定位移。
/// 
/// @throws InputError  输入数据不自洽（指针为空、载荷向量长度不符、有非零指定位移）
/// @throws SolverError 线性方程组求解失败（通常意味着结构欠约束）
StaticResult solveStatic(const StaticProblem& problem, LinearSolver& solver);

} // namespace feml
