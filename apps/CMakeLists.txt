add_executable(feml_main feml_main.cpp)
set_target_properties(feml_main PROPERTIES OUTPUT_NAME feml)
target_link_libraries(feml_main PRIVATE FEML::feml FEML::Options)