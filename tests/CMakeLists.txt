set(BINARY ${CMAKE_PROJECT_NAME}_tst)

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

add_executable(TestArgumentParser test_argument_parser.cpp)
target_link_libraries(TestArgumentParser gtest gmock gtest_main algebraic_line_filter)
add_executable(TestAlgebraicParser test_algebraic_parser.cpp)
target_link_libraries(TestAlgebraicParser gtest gmock gtest_main algebraic_line_filter)
add_executable(TestAddImpliedAND test_add_implied_and.cpp)
target_link_libraries(TestAddImpliedAND gtest gmock gtest_main algebraic_line_filter)
add_executable(TestShuntingYard test_shunting_yard.cpp)
target_link_libraries(TestShuntingYard gtest gmock gtest_main algebraic_line_filter)
add_executable(TestFilters test_filters.cpp)
target_link_libraries(TestFilters gtest gmock gtest_main algebraic_line_filter)

add_executable(test
    test_argument_parser.cpp
    test_algebraic_parser.cpp
    test_add_implied_and.cpp
    test_shunting_yard.cpp
    test_filters.cpp)

target_link_libraries(test
    algebraic_line_filter
    gtest
    gmock
    gtest_main
)
