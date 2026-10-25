# Cross-build for little-endian 64-bit POWER Linux and run the tests under QEMU as a POWER9:
#   cmake -S . -B build-ppc -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/powerpc64le-linux-gnu.cmake
#   cmake --build build-ppc && ctest --test-dir build-ppc
# Needs g++-powerpc64le-linux-gnu and qemu-user (Debian/Ubuntu package names).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR ppc64le)
set(CMAKE_C_COMPILER powerpc64le-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER powerpc64le-linux-gnu-g++)
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-ppc64le;-cpu;power9;-L;/usr/powerpc64le-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
