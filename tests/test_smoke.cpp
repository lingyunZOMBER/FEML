#include "feml/core/version.hpp"

#include <string>  
#include <catch2/catch_test_macros.hpp>

TEST_CASE("versionString returns expected format", "[core][version]") {
    const std::string version = feml::versionString();
    REQUIRE_FALSE(version.empty());
    REQUIRE(version == "0.1.0");
}