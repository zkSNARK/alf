cmake_minimum_required(VERSION 3.15)
project(algebraic_log_filter LANGUAGES CXX)
configure_file(Config.h.in Config.h)
set(CMAKE_CXX_STANDARD 20)

if (EMSCRIPTEN)
  # WebAssembly via Emscripten, run with Node.js: exceptions (alf reports a bad expression by
  # throwing), threads (a pool of Web Workers for --threads), direct access to the host's files
  # (NODERAWFS, so -i and the tests' input file work) and memory that can grow to hold a log.
  add_compile_options(-fwasm-exceptions -pthread)
  add_link_options(-fwasm-exceptions -pthread -sPTHREAD_POOL_SIZE=16 -sALLOW_MEMORY_GROWTH=1
                   -sNODERAWFS=1 -sEXIT_RUNTIME=1 -sSTACK_SIZE=1MB)
endif ()

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
