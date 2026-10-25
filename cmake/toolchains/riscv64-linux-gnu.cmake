# Cross-build for 64-bit RISC-V Linux and run the tests under QEMU with vectors enabled:
#   cmake -S . -B build-rv -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/riscv64-linux-gnu.cmake
#   cmake --build build-rv && ctest --test-dir build-rv
# Needs g++-riscv64-linux-gnu and qemu-user (Debian/Ubuntu package names).
# ALF_QEMU_VLEN picks the emulated vector length in bits (default 256).
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)
set(CMAKE_C_COMPILER riscv64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER riscv64-linux-gnu-g++)
if (NOT ALF_QEMU_VLEN)
  set(ALF_QEMU_VLEN 256)
endif ()
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-riscv64;-cpu;rv64,v=true,vlen=${ALF_QEMU_VLEN};-L;/usr/riscv64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
