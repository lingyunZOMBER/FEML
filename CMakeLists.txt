cmake_minimum_required(VERSION 3.25)

project(FEML
	VERSION 0.1.0
	DESCRIPTION "从零实现的C++有限元程序"
	LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 多配置生成器（Visual Studio）会忽略上面三条，必须按配置再设一遍
foreach(_cfg IN ITEMS DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/lib")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/lib")
endforeach()

add_library(feml_project_options INTERFACE)
add_library(FEML::Options ALIAS feml_project_options)

if(MSVC)
    target_compile_options(feml_project_options INTERFACE
        /W4              # 高警告等级，本项目要求零警告
        /permissive-     # 严格标准一致性，避免写出只在 MSVC 能编的代码
        /utf-8           # 源码与执行字符集均为 UTF-8
        /Zc:__cplusplus  # 让 __cplusplus 报告真实标准版本
        /EHsc)           # 标准 C++ 异常模型
    target_compile_definitions(feml_project_options INTERFACE
        NOMINMAX                 # 阻止 <windows.h> 的 min/max 宏污染 std::min
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS)
else()
    target_compile_options(feml_project_options INTERFACE -Wall -Wextra -Wpedantic)
endif()

include(cmake/Dependencies.cmake)

add_subdirectory(src)    # feml 静态库
add_subdirectory(apps)   # 可执行程序

enable_testing()
add_subdirectory(tests)
