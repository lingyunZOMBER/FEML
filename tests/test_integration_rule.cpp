#include "feml/element/integration_rule.hpp"

#include "feml/core/exceptions.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include <cmath>
#include <cstddef>

using Catch::Matchers::WithinAbs;

namespace {

/// 用给定规则数值积分 ξ^power
feml::Real integrateMoment(const feml::GaussRule1D& rule, int power) {
	feml::Real sum = 0.0;
	for (const auto& gp : rule.points()) {
		sum += gp.weight * std::pow(gp.xi, power);
	}
	return sum;
}

/// ∫_{-1}^{1} ξ^power dξ 的解析值：偶数次为 2/(p+1)，奇数次为 0
feml::Real exactMoment(int power) {
	return (power % 2 == 0) ? 2.0 / (power + 1) : 0.0;
}

} // namespace

TEST_CASE("weights sum to the interval length", "[integration]") {
	for (int n = 1; n <= 4; ++n) {
		INFO("n = " << n);
		const auto rule = feml::GaussRule1D::make(n);

		feml::Real sum = 0.0;
		for (const auto& gp : rule.points()) {
			sum += gp.weight;
		}
		REQUIRE_THAT(sum, WithinAbs(2.0, 1e-14));
	}
}

TEST_CASE("points and weights are symmetric about the origin", "[integration]") {
	for (int n = 1; n <= 4; ++n) {
		INFO("n = " << n);
		const auto& pts = feml::GaussRule1D::make(n).points();

		for (std::size_t i = 0; i < pts.size(); ++i) {
			const std::size_t j = pts.size() - 1 - i;
			REQUIRE_THAT(pts[i].xi, WithinAbs(-pts[j].xi, 1e-14));
			REQUIRE_THAT(pts[i].weight, WithinAbs(pts[j].weight, 1e-14));
		}
	}
}

TEST_CASE("order matches the number of points", "[integration]") {
	for (int n = 1; n <= 4; ++n) {
		const auto rule = feml::GaussRule1D::make(n);
		REQUIRE(rule.order() == n);
		REQUIRE(rule.points().size() == static_cast<std::size_t>(n));
	}
}

//  n 点高斯积分对 2n-1 次多项式精确

TEST_CASE("n-point rule integrates all polynomials up to degree 2n-1 exactly",
          "[integration]") {
	for (int n = 1; n <= 4; ++n) {
		const auto rule = feml::GaussRule1D::make(n);

		for (int p = 0; p <= 2 * n - 1; ++p) {
			INFO("n = " << n << ", power = " << p);
			REQUIRE_THAT(integrateMoment(rule, p), WithinAbs(exactMoment(p), 1e-13));
		}
	}
}

TEST_CASE("n-point rule fails to integrate degree 2n exactly", "[integration]") {
	for (int n = 1; n <= 3; ++n) {
		const auto rule = feml::GaussRule1D::make(n);
		const int p = 2 * n;   // 刚好超出精确范围一次

		INFO("n = " << n << ", power = " << p);
		REQUIRE(std::abs(integrateMoment(rule, p) - exactMoment(p)) > 1e-6);
	}
}

//  错误处理

TEST_CASE("invalid order throws InputError", "[integration]") {
	REQUIRE_THROWS_AS(feml::GaussRule1D::make(0), feml::InputError);
	REQUIRE_THROWS_AS(feml::GaussRule1D::make(5), feml::InputError);
	REQUIRE_THROWS_AS(feml::GaussRule1D::make(-1), feml::InputError);
}

TEST_CASE("integration scheme keyword parsing is case-insensitive", "[integration]") {
	using feml::IntegrationScheme;

	REQUIRE(feml::parseIntegrationScheme("FULL") == IntegrationScheme::Full);
	REQUIRE(feml::parseIntegrationScheme("full") == IntegrationScheme::Full);
	REQUIRE(feml::parseIntegrationScheme("Exact") == IntegrationScheme::Full);

	REQUIRE(feml::parseIntegrationScheme("REDUCED") == IntegrationScheme::Reduced);
	REQUIRE(feml::parseIntegrationScheme("reduced") == IntegrationScheme::Reduced);

	REQUIRE(feml::parseIntegrationScheme("SELECTIVE") == IntegrationScheme::SelectiveReduced);
	REQUIRE(feml::parseIntegrationScheme("SRI") == IntegrationScheme::SelectiveReduced);

	REQUIRE_THROWS_AS(feml::parseIntegrationScheme("NONSENSE"), feml::InputError);
}
