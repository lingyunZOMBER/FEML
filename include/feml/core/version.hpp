#pragma once

#include <string>

namespace feml {

	inline constexpr int kVersionMajor = 0;
	inline constexpr int kVersionMinor = 1;
	inline constexpr int kVersionPatch = 0;

	/// 版本号字符串，形如 "0.1.0"
	std::string versionString();

} // namespace feml