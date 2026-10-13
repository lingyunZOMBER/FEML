#pragma once

#include "feml/core/types.hpp"

#include<string>

namespace feml {

	/// 材料本构参数
	/// 现在只支持线弹性各向同性（E、ν）。

	struct Material {
		Index       id = KInvalidIndex;   ///< 输入文件中的材料号
		std::string name;                 ///< 材料名，如 "STEEL"
		Real        E = 0.0;             ///< 弹性模量 E
		Real        nu = 0.0;             ///< 泊松比 ν

		/// 剪切模量 G = E / (2(1+ν))
		Real shearModulus() const {
			return E / (2.0 * (1.0 + nu));
		}
	};

}//namespace feml

