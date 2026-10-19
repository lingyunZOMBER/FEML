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

	// 二维高斯积分：两个一维规则的张量积
	GaussRule2D GaussRule2D::make(int nx, int ny) {
		if (nx < 1 || nx > 4 || ny < 1 || ny > 4) {
			throw InputError("二维高斯积分点数 " + std::to_string(nx) + "x"
			                 + std::to_string(ny) + " 超出支持范围 [1,4]x[1,4]");
		}

		const GaussRule1D rx = GaussRule1D::make(nx);
		const GaussRule1D ry = GaussRule1D::make(ny);

		GaussRule2D rule;
		rule.points_.reserve(static_cast<std::size_t>(nx * ny));

		for (const GaussPoint1D& px : rx.points()) {
			for (const GaussPoint1D& py : ry.points()) {
				GaussPoint2D gp;
				gp.xi     = px.xi;
				gp.eta    = py.xi;
				gp.weight = px.weight * py.weight;
				rule.points_.push_back(gp);
			}
		}

		return rule;
	}

	//积分方案关键字
	IntegrationScheme parseIntegrationScheme(std::string_view text) {		const std::string key = toUpper(text);

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
