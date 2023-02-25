project(ERSM CXX)

# wxWidget options
set(wxWidgets_PREFIX "" CACHE STRING "wxWidgets ROOT dir")
option(wxWidgets_STATIC "wxWidgets link static libaries" OFF)

# find wxWidget
if(wxWidgets_PREFIX)
    set(wxWidgets_CONFIG_OPTIONS --prefix=${wxWidgets_PREFIX})
endif()
set(wxWidgets_USE_UNICODE ON)
set(wxWidgets_USE_STATIC ${wxWidgets_STATIC})
find_package(wxWidgets REQUIRED COMPONENTS core base)
include(${wxWidgets_USE_FILE})

# Check for AVX2 support
include(CheckCXXSourceRuns)
if(MSVC)
    set(CMAKE_REQUIRED_FLAGS "/arch:AVX2")
else()
    set(CMAKE_REQUIRED_FLAGS "-mavx2")
endif()
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
if(HAVE_AVX2_EXTENSIONS)
    set(MD5_SRC md5_simd.cpp md5_simd.h)
else()
    set(MD5_SRC md5.c md5.h)
endif()
add_executable(${PROJECT_NAME} main.cpp ${MD5_SRC}
    mem.cpp mem.h
    savefile.cpp savefile.h
    mainwnd.cpp mainwnd.h)
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_NAME ERSavMan
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
    FOLDER app)
target_include_directories(${PROJECT_NAME} PRIVATE .)
target_link_libraries(${PROJECT_NAME} ${wxWidgets_LIBRARIES})
if(HAVE_AVX2_EXTENSIONS)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PUBLIC /arch:AVX2)
    else()
        target_compile_options(${PROJECT_NAME} PUBLIC -mavx2)
    endif()
endif()
