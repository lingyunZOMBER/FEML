add_library(feml STATIC
    core/version.cpp
    linalg/eigen_solver.cpp
    model/dof_manager.cpp
    element/integration_rule.cpp
    element/registry.cpp
    element/beam2_timoshenko.cpp
    element/beam3_timoshenko.cpp
    element/register_builtin.cpp
    analysis/assembler.cpp
    analysis/static_analysis.cpp)

add_library(FEML::feml ALIAS feml)

target_include_directories(feml
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)

target_link_libraries(feml
    PUBLIC  FEML::Eigen
    PRIVATE FEML::Options)
