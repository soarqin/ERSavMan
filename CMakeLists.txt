cmake_minimum_required(VERSION 3.13)
project(ERSavMan)

set(CMAKE_CXX_STANDARD_REQUIRED 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(CustomCompilerOptions)
fix_compile_flags()
fix_release_flags()
add_static_runtime_option()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(deps)
add_subdirectory(src)
