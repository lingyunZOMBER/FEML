#pragma once

#include "feml/core/types.hpp"

#include<string_view>
#include<vector>

namespace feml {

//一维gauss积分（-1，1）
	struct GaussPoint1D {
		Real xi		= 0.0;
		Real weight = 0.0;
	};

	class GaussRule1D {
	public:
		//构造n点gauss规则，n（1，4）
		//@throws InputError n 超出范围
		static GaussRule1D make(int order);
		int order() const { return static_cast<int>(points_.size()); }
		const std::vector<GaussPoint1D>& points() const { return points_; }

	private:
		std::vector<GaussPoint1D> points_;
	};

	//积分方案

	enum class IntegrationScheme {
		Full,				//完全积分
		Reduced,			//减缩积分
		SelectiveReduced,	//选择性减缩积分（只对剪切项降阶）
	};

	//@throws InputError 关键字无法识别
	IntegrationScheme parseIntegrationScheme(std::string_view text);

}//namespace feml