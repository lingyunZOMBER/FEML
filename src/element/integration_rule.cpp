#include "feml/element/integration_rule.hpp"

#include "feml/core/exceptions.hpp"

#include<algorithm>
#include<cctype>
#include<cmath>
#include<numbers>
#include<string>

namespace feml {
	namespace {
		//关键字转大写
		std::string toUpper(std::string_view text) {
			std::string out(text);
			std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {return static_cast<char>(std::toupper(c));});
			return out;
		}
	}//namespace

	//Gauss积分
	GaussRule1D GaussRule1D::make(int order) {
		GaussRule1D rule;

		switch (order) {
			case 1:
				//精确到1次
				rule.points_ = { {0.0, 2.0} };
				break;
			case 2: {
				//精确到3次
				const Real a = 1.0 / std::numbers::sqrt3;
				rule.points_ = { {-a,1.0},{a,1.0} };
				break;
			}
			case 3: {
				//精确到5次
				const Real a = std::sqrt(3.0 / 5.0);
				rule.points_ = { {-a,5.0 / 9.0},{0.0, 8.0 / 9.0},{a,5.0 / 9.0 } };
				break;
			}
			case 4: {
				// 精确到 7 次。4 点没有简洁的闭式表达，直接写数值
				constexpr Real kA = 0.3399810435848563;
				constexpr Real kWA = 0.6521451548625461;
				constexpr Real kB = 0.8611363115940526;
				constexpr Real kWB = 0.3478548451374538;
				rule.points_ = { {-kB, kWB}, {-kA, kWA}, {kA, kWA}, {kB, kWB} };
				break;
			}

			default:
				throw InputError("高斯积分点数" + std::to_string(order) + "超出支持范围[1, 4]");
		}
		return rule;
	}

	//积分方案关键字
	IntegrationScheme parseIntegrationScheme(std::string_view text) {
		const std::string key = toUpper(text);

		if (key == "FULL" || key == "EXACT") {
			return IntegrationScheme::Full;
		}
		if (key == "REDUCED") {
			return IntegrationScheme::Reduced;
		}
		if (key == "SELECTIVE" || key == "SELECTIVE_REDUCED" || key == "SRI") {
			return IntegrationScheme::SelectiveReduced;
		}

		throw InputError("未知的积分方案\"" + std::string(text) + "\",可选:FULL、REDUCED、SELECTIVE");
	}
} //namespace feml
