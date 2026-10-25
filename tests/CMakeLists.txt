include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# The argument-parser tests open tests/infile.txt, so every test runs from the build directory
# (the top-level CMakeLists copies the file there).
function(alf_test name source)
  add_executable(${name} ${source})
  target_link_libraries(${name} PRIVATE algebraic_line_filter GTest::gtest GTest::gmock GTest::gtest_main)
  add_test(NAME ${name} COMMAND ${name} WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
endfunction()

alf_test(TestArgumentParser test_argument_parser.cpp)
alf_test(TestAlgebraicParser test_algebraic_parser.cpp)
alf_test(TestAddImpliedAND test_add_implied_and.cpp)
alf_test(TestShuntingYard test_shunting_yard.cpp)
alf_test(TestFilters test_filters.cpp)
alf_test(TestExpressions test_expressions.cpp)
alf_test(TestProgram test_program.cpp)
alf_test(TestSimd test_simd.cpp)
target_include_directories(TestSimd PRIVATE ${PROJECT_SOURCE_DIR}/libalf/src/simd)

# The whole filter test suite once per SIMD backend (ALF_SIMD forces it; a backend this machine
# can't run skips itself, see SimdEnvironment in test_program.cpp).
foreach (backend scalar sse2 ssse3 avx2 avx512 neon sve rvv vsx wasm)
  add_test(NAME TestProgram_${backend} COMMAND TestProgram WORKING_DIRECTORY ${PROJECT_BINARY_DIR})
  set_tests_properties(TestProgram_${backend} PROPERTIES ENVIRONMENT "ALF_SIMD=${backend}" SKIP_REGULAR_EXPRESSION "doesn't run on this machine")
endforeach ()
