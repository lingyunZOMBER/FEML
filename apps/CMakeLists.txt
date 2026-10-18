add_executable(feml_main feml_main.cpp)
set_target_properties(feml_main PROPERTIES OUTPUT_NAME feml)
target_link_libraries(feml_main PRIVATE FEML::feml FEML::Options)

# 作业 2：梁单元批量扫描
add_executable(beam_sweep beam_sweep.cpp)
target_link_libraries(beam_sweep PRIVATE FEML::feml FEML::Options)

# 作业 3：板单元批量扫描
add_executable(plate_sweep plate_sweep.cpp)
target_link_libraries(plate_sweep PRIVATE FEML::feml FEML::Options)
