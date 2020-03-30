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
target_link_libraries(TestArgumentParser gtest gmock gtest_main libalf)
add_executable(TestAlgebraicParser test_algebraic_parser.cpp)
target_link_libraries(TestAlgebraicParser gtest gmock gtest_main libalf)
add_executable(TestAddImplliedAND  test_add_implied_and.cpp)
target_link_libraries(TestAddImplliedAND gtest gmock gtest_main libalf)

add_executable(AllTest
    test_argument_parser.cpp
    test_algebraic_parser.cpp
    test_add_implied_and.cpp
)
target_link_libraries(AllTest
    libalf
    gtest
    gmock
    gtest_main
)
