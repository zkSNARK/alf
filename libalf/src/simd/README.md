# alf's SIMD layer

alf scans bytes in three places where vector instructions pay off: looking for any of a few
bytes (the rare bytes of the strings being searched for), Teddy fingerprint lookups (many
strings at once), and walking back to the start of a line. Each of those is a **kernel**, and
each instruction set has its own implementation of all three, chosen once at run time.

| Backend | Architecture | Width | Teddy | Chosen automatically |
| --- | --- | --- | --- | --- |
| `avx512` | x86-64, AVX-512 F + BW | 64 bytes | yes | no (see below); `ALF_SIMD=avx512` |
| `avx2` | x86-64, AVX2 | 32 bytes | yes | yes |
| `ssse3` | x86-64, SSSE3 | 16 bytes | yes | yes |
| `sse2` | x86-64 (all) | 16 bytes | scalar | yes |
| `sve` | AArch64 with SVE / SVE2 | 16–256 bytes | yes | when its vectors are wider than 16 bytes |
| `neon` | AArch64 (all) | 16 bytes | yes | yes |
| `rvv` | RISC-V with the V extension (RVV 1.0) | VLEN / 8 bytes | yes | yes |
| `vsx` | little-endian POWER8+ | 16 bytes | yes | yes |
| `wasm` | WebAssembly SIMD128 (Emscripten) | 16 bytes | yes | always, when built in (see below) |
| `scalar` | anything | 1 byte | scalar | last resort |

`alf --simd-info` prints which backends a binary has, which run on this CPU and which is in use.

## How it fits together

- **`simd.h`** is the interface: plain data (`ByteSet`, `TeddyTables`), the three kernel
  function types, and `Backend`, a record of a backend's kernels plus "does it run here" and
  "pick it automatically".
- **One file per instruction set** (`x86_sse2.cpp`, `x86_ssse3.cpp`, `x86_avx2.cpp`,
  `x86_avx512.cpp`, `arm_neon.cpp`, `arm_sve.cpp`) implements the kernels with intrinsics. CMake
  compiles each file with only its own flags, and only on the matching architecture, so one
  binary carries every backend for its architecture and runs on any CPU of it.
- **`scalar.cpp`** is the reference. Every other backend must return exactly what it returns:
  `tests/test_simd.cpp` checks that on random inputs, and the vector kernels use the scalar ones
  for the few bytes at the end of a range that a whole vector can't cover.
- **`dispatch.cpp`** detects the CPU (`__builtin_cpu_supports` on x86, which also checks that the
  OS saves the wide registers; `getauxval(AT_HWCAP)` for SVE on Linux), builds the backend
  table, and picks the first backend that runs here and is preferred. It's compiled with the
  baseline flags only, so the checks never use the instructions they check for.
- The scanner (`program.cpp`) asks `alf::simd::active()` once per filter and calls through the
  function pointers. There is no `#ifdef` in the scanner and no feature check per call.

## Forcing a backend

`ALF_SIMD=<name>` picks a backend, if it runs on this machine (otherwise alf says so on stderr
and chooses automatically). ctest runs the whole filter test suite once per backend this way
(`TestProgram_avx2`, `TestProgram_neon`, ...), skipping the ones the machine can't run, so CI on
x86 and ARM covers every backend those machines have.

## The ODR trap (why these files look the way they do)

A file compiled with `-mavx2` may only be entered after the CPU has been checked. But if it uses
an inline function or template from a header (`std::min`, a lambda passed to an algorithm, any
inline helper), the compiler emits a copy of that function compiled *with AVX2*, and the linker
is allowed to keep that copy for the whole program. Some other, unchecked caller then runs AVX2
instructions on a CPU without them and crashes.

So the backend files use intrinsics, local lambdas and helpers in an anonymous namespace only,
never standard-library templates; and `simd.h` and `kernels.h` contain declarations and plain
data, nothing inline.

## Why AVX-512 isn't automatic

On several generations of Intel CPUs, running 512-bit instructions lowers the clock speed of
that core for a while, which can make everything else slower than the vector code saved. That
depends on the CPU and hasn't been measured for alf yet, so `avx512` is built, tested in CI
where the runner has it, and available with `ALF_SIMD=avx512`, but `avx2` is the default.

## Adding a backend

1. Write `<arch>_<isa>.cpp` implementing the three kernels (the scalar ones are the
   specification; reuse them for the tail of a range).
2. Declare them in `kernels.h` under a new `ALF_SIMD_<ISA>` guard.
3. Add a detection function and a `Backend` entry to `dispatch.cpp`, in order of preference.
4. Add the file to `libalf/CMakeLists.txt` with its flags (a `check_cxx_source_compiles` guard
   if older compilers may not know them) and the name to the `foreach` in
   `tests/CMakeLists.txt`.

## Eventually: `std::simd`

C++26's `std::simd` (P1928) is the intended long-term replacement for the per-instruction-set
files: the same kernel source, compiled once per target. Two things keep this layer for now.

- **Run-time choice.** `std::simd` picks its width at compile time. Choosing the best
  instruction set for the CPU at run time would still need this dispatch layer, selecting
  between copies of the `std::simd` code compiled for different targets.
- **A byte shuffle with run-time indices.** Teddy's table lookup is `pshufb` / `tbl`. The
  `std::simd` permute facilities cover compile-time patterns first; a run-time
  table lookup isn't something portable `std::simd` code can rely on yet.

When the compilers alf supports ship both, each backend file becomes the same `std::simd`
kernel built for a different target, and `dispatch.cpp` and the tests stay as they are.

## WebAssembly

WebAssembly can't test for SIMD at run time: a module that contains SIMD instructions needs an
engine that supports them, which every current browser and runtime does. So the `wasm` backend
is a build-time choice, on by default; `-DALF_WASM_SIMD=OFF` builds a module without it. alf
builds with Emscripten (`emcmake cmake ...`) and runs under Node.js with exceptions, threads (a
pool of Web Workers) and direct access to the host's files; see the top-level `CMakeLists.txt`.

## Testing architectures you don't have

`cmake/toolchains/` has cross-compile toolchain files for RISC-V (`riscv64-linux-gnu`) and
POWER (`powerpc64le-linux-gnu`). They set `CMAKE_CROSSCOMPILING_EMULATOR` to QEMU, so `ctest`
runs every test on an emulated CPU (RISC-V with vectors, `ALF_QEMU_VLEN` bits wide; POWER9).
CI does this on every push, and also builds the WebAssembly version and runs its tests under
Node.js. RVV code must be correct at every vector length; the kernel tests
have been run at 128, 256, 512 and 1024 bits, and on a RISC-V CPU without vectors (scalar).
