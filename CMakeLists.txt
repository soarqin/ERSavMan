set(CMAKE_WARN_DEPRECATED OFF CACHE INTERNAL "")

cmake_minimum_required(VERSION 3.21)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
include(GlobalOptions)
add_global_options()
include(CustomCompilerOptions)
fix_compile_flags()
fix_release_flags(STRIP_BINARY ON USE_LTO OFF USE_STATIC_CRT ON)
include(ProjectMacros)

project(ERSavMan)

add_subdirectory(src)
