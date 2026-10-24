cmake_minimum_required(VERSION 3.15)
project(algebraic_log_filter LANGUAGES CXX)
configure_file(Config.h.in Config.h)
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(libalf)

add_executable(alf src/main.cpp)

target_link_libraries(alf
    PRIVATE algebraic_line_filter
)
target_include_directories(alf PUBLIC "${PROJECT_BINARY_DIR}")

option(ALF_BUILD_TESTS "Build the unit tests" ON)
if (ALF_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
  file(COPY tests/infile.txt DESTINATION ${PROJECT_BINARY_DIR}/tests)
endif ()
