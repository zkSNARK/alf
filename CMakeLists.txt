cmake_minimum_required(VERSION 3.15)
project(algebraic_log_filter)
configure_file(Config.h.in Config.h)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(libalf)

add_executable(sy tests/test_shunting_yard.cpp)
target_link_libraries(sy libalf)

add_executable(alf src/alf.cpp)

target_link_libraries(alf
    PRIVATE libalf
    )
target_include_directories(alf PUBLIC "${PROJECT_BINARY_DIR}")

add_subdirectory(tests)

file(COPY tests/infile.txt DESTINATION ${PROJECT_BINARY_DIR}/tests)