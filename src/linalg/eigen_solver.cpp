#include "feml/linalg/eigen_solver.hpp"

#include "feml/core/exceptions.hpp"

#include <Eigen/Cholesky>
#include <Eigen/LU>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <string>

namespace feml {
namespace {

/// 尺寸不匹配属于使用错误，直接抛异常。
void checkDimensions(const Eigen::MatrixXd& K, const Eigen::VectorXd& F) {
	if (K.rows() != K.cols()) {
		throw SolverError("刚度矩阵不是方阵：" + std::to_string(K.rows())
		                  + " x " + std::to_string(K.cols()));
	}
	if (K.rows() != F.size()) {
		throw SolverError("刚度矩阵维数 " + std::to_string(K.rows())
		                  + " 与载荷向量长度 " + std::to_string(F.size()) + " 不一致");
	}
}

} // namespace

bool EigenLdltSolver::solve(const Eigen::MatrixXd& K,
                            const Eigen::VectorXd& F,
                            Eigen::VectorXd& u) {
	checkDimensions(K, F);

	if (K.rows() == 0) {
		u.resize(0);
		return true;
	}

	const Eigen::LDLT<Eigen::MatrixXd> ldlt(K);

	// D 中接近零的对角元 ⇒ 矩阵奇异或非正定。
	//
	// 典型原因：结构欠约束（存在刚体位移模式），或单元积分不足产生了零能模态。

	const Eigen::VectorXd& D = ldlt.vectorD();
	const double maxAbsD = D.cwiseAbs().maxCoeff();
	const double tol = maxAbsD * std::numeric_limits<double>::epsilon()
	                 * static_cast<double>(std::max<Eigen::Index>(K.rows(), 1));

	for (Eigen::Index i = 0; i < D.size(); ++i) {
		if (std::abs(D(i)) <= tol) {
			return false;
		}
	}

	u = ldlt.solve(F);
	return ldlt.info() == Eigen::Success && u.allFinite();
}

bool EigenLuSolver::solve(const Eigen::MatrixXd& K,
                          const Eigen::VectorXd& F,
                          Eigen::VectorXd& u) {
	checkDimensions(K, F);

	if (K.rows() == 0) {
		u.resize(0);
		return true;
	}

	const Eigen::FullPivLU<Eigen::MatrixXd> lu(K);
	if (!lu.isInvertible()) {
		return false;
	}

	u = lu.solve(F);

	// 注意：FullPivLU 没有 info() 成员（LDLT 才有），
	// 所以这里只能靠"解是否有限"来判断。非有限值（inf / nan）
	// 通常意味着矩阵严重病态。
	return u.allFinite();
}

} // namespace feml
