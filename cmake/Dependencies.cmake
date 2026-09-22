include(FetchContent)

FetchContent_Declare(Eigen
    URL      https://gitlab.com/libeigen/eigen/-/archive/3.4.1/eigen-3.4.1.tar.gz
    URL_HASH SHA256=b93c667d1b69265cdb4d9f30ec21f8facbbe8b307cf34c0b9942834c6d4fdbe2
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR  cmake-not-used)      

FetchContent_Declare(Catch2
    URL      https://github.com/catchorg/Catch2/archive/refs/tags/v3.9.1.tar.gz
    URL_HASH SHA256=a215c2a723bd7483efd236dc86066842a389cb4e344c61119c978acdf24d39be
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)

set(CATCH_BUILD_TESTING     OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_DOCS      OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS    OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(Eigen Catch2)


#  包装成项目自己的目标

add_library(feml_eigen INTERFACE)
target_include_directories(feml_eigen SYSTEM INTERFACE "${eigen_SOURCE_DIR}")
add_library(FEML::Eigen ALIAS feml_eigen)

# Catch2 已经是正式的 target，不需要包装，只需把它的头文件目录标成 SYSTEM
get_target_property(_catch2_real Catch2::Catch2 ALIASED_TARGET)
if(NOT _catch2_real)
    set(_catch2_real Catch2::Catch2)   # 万一它不是别名，就用它本身
endif()

get_target_property(_catch2_incs ${_catch2_real} INTERFACE_INCLUDE_DIRECTORIES)
if(_catch2_incs)
    set_target_properties(${_catch2_real} PROPERTIES
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_catch2_incs}")
endif()
