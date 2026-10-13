#include "feml/element/registry.hpp"

#include "feml/core/exceptions.hpp"

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace feml {
	namespace {

		/// 关键字统一转大写
		std::string toUpper(std::string_view text) {
			std::string out(text);
			std::transform(out.begin(), out.end(), out.begin(),
				[](unsigned char c) { return static_cast<char>(std::toupper(c)); });
			return out;
		}

		/// 把所有已登记的名字拼成一行，用于错误提示
		std::string availableList() {
			const std::vector<std::string> all = ElementRegistry::names();
			if (all.empty()) {
				return "(尚未登记任何单元)";
			}
			std::string s;
			for (std::size_t i = 0; i < all.size(); ++i) {
				if (i > 0) {
					s += "、";
				}
				s += all[i];
			}
			return s;
		}

	} // namespace

	std::unordered_map<std::string, ElementRegistry::Factory>& ElementRegistry::table() {
		static std::unordered_map<std::string, Factory> instance;
		return instance;
	}

	void ElementRegistry::add(std::string name, Factory factory) {
		const std::string key = toUpper(name);

		auto& t = table();
		if (t.find(key) != t.end()) {
			throw InputError("单元类型 \"" + name + "\" 重复登记");
		}
		t.emplace(key, std::move(factory));
	}

	std::unique_ptr<Element> ElementRegistry::create(std::string_view name) {
		const std::string key = toUpper(name);

		const auto& t = table();
		const auto it = t.find(key);
		if (it == t.end()) {
			throw InputError("未知的单元类型 \"" + std::string(name)
				+ "\"，可用：" + availableList());
		}
		return it->second();
	}

	bool ElementRegistry::contains(std::string_view name) {
		const auto& t = table();
		return t.find(toUpper(name)) != t.end();
	}

	std::vector<std::string> ElementRegistry::names() {
		const auto& t = table();

		std::vector<std::string> out;
		out.reserve(t.size());
		for (const auto& entry : t) {
			out.push_back(entry.first);
		}
		std::sort(out.begin(), out.end());
		return out;
	}

} // namespace feml