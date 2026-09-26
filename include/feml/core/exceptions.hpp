#pragma once
#include <stdexcept>
#include <string>

namespace feml {

	//所有FEML异常的基类
	class FemlError : public std::runtime_error {
	public:
		explicit FemlError(const std::string& message) : std::runtime_error(message) {}
	};

	//输入文件错误：关键字拼错、节点编号越界、材料未定义、数据个数不符……
	class InputError : public FemlError {
	public:
		explicit InputError(const std::string& message) : FemlError("输入错误: " + message) {}
	};

	// 求解过程错误：刚度矩阵奇异、结构欠约束、迭代不收敛……
	class SolverError : public FemlError {
	public:
		explicit SolverError(const std::string& message)
			: FemlError("求解错误: " + message) {
		}
	};

	/// 访问了尚未实现的功能。
	class NotImplementedError : public FemlError {
	public:
		explicit NotImplementedError(const std::string& message)
			: FemlError("尚未实现: " + message) {
		}
	};
} //namespace feml
