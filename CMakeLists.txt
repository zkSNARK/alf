cmake_minimum_required(VERSION 3.15)
project(algebraic_log_filter)

set(CMAKE_CXX_STANDARD 20)

add_subdirectory(libalf)

add_executable(sy tests/shuntingyard.cpp)
target_link_libraries(sy libalf)

add_executable(alf src/alf.cpp)

target_link_libraries(alf
    PRIVATE libalf
)

include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG release-1.8.0
)

FetchContent_MakeAvailable(googletest)

set_target_properties(gtest PROPERTIES FOLDER extern)
set_target_properties(gtest_main PROPERTIES FOLDER extern)
set_target_properties(gmock PROPERTIES FOLDER extern)
set_target_properties(gmock_main PROPERTIES FOLDER extern)

add_executable(TestArgumentParser tests/test_argument_parser.cpp)
target_link_libraries(TestArgumentParser gtest gmock gtest_main libalf)
add_executable(TestAlgebraicParser tests/test_algebraic_parser.cpp)
target_link_libraries(TestAlgebraicParser gtest gmock gtest_main libalf)
add_executable(TestAddImplliedAND  tests/test_add_implied_and.cpp)
target_link_libraries(TestAddImplliedAND gtest gmock gtest_main libalf)

add_executable(AllTest
    tests/test_argument_parser.cpp
    tests/test_algebraic_parser.cpp
    tests/test_add_implied_and.cpp
)
target_link_libraries(AllTest
    libalf
    gtest
    gmock
    gtest_main
)
