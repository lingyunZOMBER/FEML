#pragma once

#include "feml/core/types.hpp"

namespace feml {
	/// 截面 / 几何性质。

	struct Section {
		Index id = KInvalidIndex;

		// --- 梁单元 ---
		Real area = 0.0;        ///< 截面面积 A
		Real inertia = 0.0;        ///< 截面惯性矩 I
		Real shearFactor = 5.0 / 6.0;  ///< 剪切修正因子 k（矩形截面 5/6）

		// --- 平面单元 ---
		Real thickness = 1.0;
	};
}//namespace feml