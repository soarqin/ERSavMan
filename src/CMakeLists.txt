cmake_minimum_required(VERSION 3.28)

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(wxWidgets_USE_DEBUG ON)
    set(wxWidgets_USE_STATIC OFF)
else ()
    set(wxWidgets_USE_DEBUG OFF)
    set(wxWidgets_USE_STATIC ON)
endif ()
set(wxWidgets_USE_UNICODE ON)
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# Check for AVX2 support
include(CheckCXXSourceRuns)
if (MSVC)
    set(CMAKE_REQUIRED_FLAGS "/arch:AVX2")
else ()
    set(CMAKE_REQUIRED_FLAGS "-mavx2")
endif ()
check_cxx_source_runs("#include <immintrin.h>
int main() {
    __m256i a, b, c;
    const int src[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    int dst[8];
    a =  _mm256_loadu_si256((__m256i*)src);
    b =  _mm256_loadu_si256((__m256i*)src);
    c = _mm256_add_epi32(a, b);
    _mm256_storeu_si256((__m256i*)dst, c);
    for(int i = 0; i < 8; i++) {
        if((src[i] + src[i]) != dst[i]) {
            return -1;
        }
    }
    return 0;
}" HAVE_AVX2_EXTENSIONS)

# build
if (HAVE_AVX2_EXTENSIONS)
    set(MD5_SRC md5_simd.cpp md5_simd.h)
else ()
    set(MD5_SRC md5.c md5.h)
endif ()
add_project(ERSM WIN32 main.cpp ${MD5_SRC}
    mem.cpp mem.h
    savefile.cpp savefile.h
    mainwnd.cpp mainwnd.h

    res/manifest.rc

    FOLDER "app"
    OUTPUT_NAME "ERSavMan")

if (HAVE_AVX2_EXTENSIONS)
    if (MSVC)
        target_compile_options(${PROJECT_NAME} PUBLIC /arch:AVX2)
    else ()
        target_compile_options(${PROJECT_NAME} PUBLIC -mavx2)
    endif ()
endif ()

target_include_directories(${PROJECT_NAME} PRIVATE .)
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})
if (wxWidgets_LIB_DIR MATCHES "_lib$")
    target_link_libraries(${PROJECT_NAME} gdiplus msimg32)
endif ()
