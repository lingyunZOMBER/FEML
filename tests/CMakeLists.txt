add_executable(feml_tests
    test_smoke.cpp
    test_integration_rule.cpp)

target_link_libraries(feml_tests
    PRIVATE
        FEML::feml
        Catch2::Catch2WithMain
        FEML::Options)

include(Catch)
catch_discover_tests(feml_tests)