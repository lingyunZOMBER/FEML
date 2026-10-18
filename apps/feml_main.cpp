#include "feml/core/exceptions.hpp"
#include "feml/core/version.hpp"
#include "feml/element/register_builtin.hpp"
#include "feml/element/registry.hpp"

#include <Eigen/Core>

#include <cstddef>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

namespace {

/// 把已注册的单元名打印成一行，方便确认注册是否成功
void printRegisteredElements() {
	const std::vector<std::string> names = feml::ElementRegistry::names();

	std::cout << "已注册的单元: ";
	if (names.empty()) {
		std::cout << "(无)";
	} else {
		for (std::size_t i = 0; i < names.size(); ++i) {
			if (i > 0) {
				std::cout << ", ";
			}
			std::cout << names[i];
		}
	}
	std::cout << "\n";
}

} // namespace

int main() {
	try {
		std::cout << "FEML " << feml::versionString() << "\n";
		std::cout << "Eigen " << EIGEN_WORLD_VERSION << "."
		          << EIGEN_MAJOR_VERSION << "." << EIGEN_MINOR_VERSION << "\n";

		feml::registerBuiltinElements();

		printRegisteredElements();

		return 0;
	}

	catch (const feml::FemlError& e) {
		std::cerr << e.what() << "\n";
		return 1;
	}
	catch (const std::exception& e) {
		std::cerr << "未预期的错误: " << e.what() << "\n";
		return 1;
	}
}
