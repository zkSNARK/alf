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
)
find_package(Threads REQUIRED)
target_link_libraries(algebraic_line_filter PUBLIC Threads::Threads)

target_include_directories(algebraic_line_filter
    PUBLIC include
)

