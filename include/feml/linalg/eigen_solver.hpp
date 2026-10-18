#pragma once

#include "feml/linalg/solver.hpp"

namespace feml {

/// 基于 Eigen 的 LDLᵀ 分解求解器。

class EigenLdltSolver final : public LinearSolver {
public:
	bool solve(const Eigen::MatrixXd& K,
	           const Eigen::VectorXd& F,
	           Eigen::VectorXd& u) override;

	std::string_view name() const override { return "EIGEN_LDLT"; }
};

/// 基于 Eigen 的完全主元 LU 分解求解器。
///
/// 用于 K 非对称或非正定的场合（例如以后引入罚函数、拉格朗日乘子，
/// 或做接触、非关联塑性问题）。比 LDLᵀ 慢，但对矩阵没有额外要求。
class EigenLuSolver final : public LinearSolver {
public:
	bool solve(const Eigen::MatrixXd& K,
	           const Eigen::VectorXd& F,
	           Eigen::VectorXd& u) override;

	std::string_view name() const override { return "EIGEN_LU"; }
};

} // namespace feml
