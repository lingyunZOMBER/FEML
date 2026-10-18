#pragma once

#include <Eigen/Core>

#include <string_view>

namespace feml {

/// 线性方程组 K·u = F 的抽象求解接口。
/// 上层（静态分析驱动）只依赖这个接口，换求解器不需要改动任何流程代码。
class LinearSolver {
public:
	virtual ~LinearSolver() = default;

	/// 求解 K·u = F。
	///
	/// @param K 系数矩阵（方阵）
	/// @param F 右端向量，长度必须等于 K 的行数
	/// @param u 输出解；实现内部按需 resize，调用方不必预先分配
	/// @return 求解成功返回 true；矩阵奇异或分解失败返回 false
	///
	/// 约定：本函数【不用异常表示数值失败】，而是返回 false，
	/// 由调用方决定是报错退出还是换一种求解策略重试。
	/// （尺寸不匹配这类使用错误仍然抛异常。）
	virtual bool solve(const Eigen::MatrixXd& K,
	                   const Eigen::VectorXd& F,
	                   Eigen::VectorXd& u) = 0;

	/// 求解器名字，用于日志与结果文件，如 "EIGEN_LDLT"
	virtual std::string_view name() const = 0;
};

} // namespace feml
