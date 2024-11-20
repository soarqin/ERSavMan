include(CMakeParseArguments)

macro(add_global_options)
    cmake_parse_arguments(OPT "" "C_STANDARD;CXX_STANDARD" "" ${ARGN})
    set(CMAKE_POLICY_DEFAULT_CMP0063 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0076 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
    set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMake")

    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)

    if (C_STANDARD)
        set(CMAKE_C_STANDARD_REQUIRED ${C_STANDARD})
    endif ()
    if (CXX_STANDARD)
        set(CMAKE_CXX_STANDARD_REQUIRED ${CXX_STANDARD})
    endif ()
endmacro()