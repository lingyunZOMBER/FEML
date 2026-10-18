add_executable(feml_main feml_main.cpp)
set_target_properties(feml_main PROPERTIES OUTPUT_NAME feml)
target_link_libraries(feml_main PRIVATE FEML::feml FEML::Options)

# 批量扫描：跑完作业要求的全部参数组合，输出 CSV 供 MATLAB/Python 绘图
add_executable(beam_sweep beam_sweep.cpp)
target_link_libraries(beam_sweep PRIVATE FEML::feml FEML::Options)
