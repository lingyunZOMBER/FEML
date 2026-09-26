add_library(feml STATIC
    core/version.cpp "element/integration_rule.cpp")
add_library(FEML::feml ALIAS feml)

target_include_directories(feml
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)

target_link_libraries(feml
    PUBLIC  FEML::Eigen
    PRIVATE FEML::Options)