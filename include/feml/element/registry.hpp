#pragma once

#include "feml/element/element.hpp"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace feml {

	/// 单元工厂注册表：把输入文件里的单元类型名映射到具体的单元类
	///
	/// 用法：
	///     ElementRegistry::add("BEAM2", [] { return std::make_unique<Beam2Timoshenko>(); });
	///
	/// 名字比较时统一转成大写
	class ElementRegistry {
	public:
		using Factory = std::function<std::unique_ptr<Element>()>;

		/// 登记一个单元类型。
		/// @throws InputError 名字重复
		static void add(std::string name, Factory factory);

		/// 按名字创建单元。
		/// @throws InputError 名字未登记
		static std::unique_ptr<Element> create(std::string_view name);

		/// 该名字是否已登记
		static bool contains(std::string_view name);

		/// 已登记的全部单元名（大写、字典序），用于报错时列出可用选项
		static std::vector<std::string> names();

	private:
		/// 全局注册表。用函数内静态变量实现，见 .cpp 里的说明。
		static std::unordered_map<std::string, Factory>& table();
	};

} // namespace feml