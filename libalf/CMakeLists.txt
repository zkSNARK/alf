cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)

add_library(algebraic_line_filter)

target_sources(algebraic_line_filter
    PUBLIC include/alf_types.h

    PUBLIC include/line_filter.h
    PRIVATE src/line_filter.cpp
    PUBLIC include/parser.h
    PRIVATE src/parser.cpp
    PUBLIC include/shunting_yard.h
    PRIVATE src/shunting_yard.cpp
    PUBLIC include/program.h
    PRIVATE src/program.cpp
    PRIVATE src/simd/dispatch.cpp
    PRIVATE src/simd/scalar.cpp
)
find_package(Threads REQUIRED)
target_link_libraries(algebraic_line_filter PUBLIC Threads::Threads)

target_include_directories(algebraic_line_filter
    PUBLIC include
    PRIVATE src/simd
)

# ---- SIMD backends ---------------------------------------------------------------------------
# One file per instruction set, each compiled with only its own flags, so the binary runs on any
# CPU of the architecture and picks the best backend at run time (src/simd/README.md).  Flags are
# GCC/Clang spellings; other compilers get the scalar backend only.
include(CheckCXXSourceCompiles)
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" alf_cpu)
if (CMAKE_OSX_ARCHITECTURES)
  string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" alf_cpu)
endif ()
set(alf_gnu_like OFF)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  set(alf_gnu_like ON)
endif ()

function(alf_simd_source file flags)
  target_sources(algebraic_line_filter PRIVATE ${file})
  set_source_files_properties(${file} PROPERTIES COMPILE_OPTIONS "${flags}")
endfunction()

if (alf_gnu_like AND alf_cpu MATCHES "^(x86_64|amd64|i[3-6]86)$")
  alf_simd_source(src/simd/x86_sse2.cpp "-msse2")
  alf_simd_source(src/simd/x86_ssse3.cpp "-mssse3")
  alf_simd_source(src/simd/x86_avx2.cpp "-mavx2")
  target_compile_definitions(algebraic_line_filter PRIVATE ALF_SIMD_X86)
  set(CMAKE_REQUIRED_FLAGS "-mavx512f -mavx512bw")
  check_cxx_source_compiles("#include <immintrin.h>
    int main() { __m512i v = _mm512_set1_epi8(1); return (int)_mm512_test_epi8_mask(v, v); }" ALF_CAN_AVX512)
  unset(CMAKE_REQUIRED_FLAGS)
  if (ALF_CAN_AVX512)
    alf_simd_source(src/simd/x86_avx512.cpp "-mavx512f;-mavx512bw")
    target_compile_definitions(algebraic_line_filter PRIVATE ALF_SIMD_AVX512)
  endif ()
elseif (alf_gnu_like AND alf_cpu MATCHES "^(aarch64|arm64)$")
  alf_simd_source(src/simd/arm_neon.cpp "")
  target_compile_definitions(algebraic_line_filter PRIVATE ALF_SIMD_NEON)
  set(CMAKE_REQUIRED_FLAGS "-march=armv8-a+sve")
  check_cxx_source_compiles("#include <arm_sve.h>
    int main() { return (int)svcntb(); }" ALF_CAN_SVE)
  unset(CMAKE_REQUIRED_FLAGS)
  if (ALF_CAN_SVE)
    alf_simd_source(src/simd/arm_sve.cpp "-march=armv8-a+sve")
    target_compile_definitions(algebraic_line_filter PRIVATE ALF_SIMD_SVE)
  endif ()
endif ()
