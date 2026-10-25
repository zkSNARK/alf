//
// simd.h -- alf's SIMD layer: a few byte-scanning kernels, one implementation per instruction
// set, chosen once at run time.
//
// Design (see README.md in this directory for the long version):
//
//  - Each instruction set has its own .cpp file, compiled with only that instruction set's
//    flags (-mavx2, +sve, ...). So one binary carries every backend and runs on any CPU of the
//    architecture; nothing outside these files is compiled for newer instructions.
//  - dispatch.cpp detects the CPU once and picks the best backend; the scanner calls the
//    kernels through the Backend's function pointers.  ALF_SIMD=<name> forces a backend (tests).
//  - scalar.cpp is the reference: every other backend must return exactly what it returns.
//
// This header is deliberately plain: types with no member functions and function-pointer
// declarations only.  Anything inline here would be compiled into the AVX2/AVX-512/SVE files
// too, and the linker may keep that copy for the whole program (see README.md, "the ODR trap").
//
// Eventually: C++26 std::simd (P1928) should replace the per-instruction-set files.  What it
// doesn't cover yet, and this layer does: choosing an instruction set at run time (std::simd is
// compile-time; the dispatch here would stay, selecting std::simd code built per target), and a
// byte shuffle with run-time indices, which Teddy needs (std::simd's permute proposals cover
// compile-time patterns first).  When both are available in the compilers alf supports, each
// backend file becomes the same std::simd source compiled for a different target.
//

#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace alf::simd
{
  /** A set of bytes to look for: `bytes[0..count)` are the members, `member[b]` the same as a table. */
  struct ByteSet
  {
    unsigned char bytes[256];
    unsigned count;
    bool member[256];
  };

  /**
   * Teddy tables (the shuffle-based multi-string fingerprint from Hyperscan, also used by
   * ripgrep).  For fingerprint byte k in 0..2 and half-byte value n, lo[k][n] / hi[k][n] have
   * bit g set if some string in group g has a byte whose low / high 4 bits are n at position k.
   * A position starts a candidate if, for all three k, lo[k][low(b)] & hi[k][high(b)] keeps a bit.
   */
  struct TeddyTables
  {
    alignas(64) unsigned char lo[3][16];
    alignas(64) unsigned char hi[3][16];
  };

  // ---- kernels ----------------------------------------------------------------------------
  // All take a range [p, last) to report positions in, and a `limit` (>= last, in the same
  // buffer) up to which bytes may be read: reading past `last` lets a short range still be
  // checked a whole vector at a time.  None of them read before `p` (except line_start, which
  // reads [floor, p)) or at or past `limit`.

  /** First position in [p, last) whose byte is in `set`, or `last`. */
  using FindAnyFn = char const* (*)(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept;

  /**
   * First position q in [p, last) where some Teddy group survives all three lookups, using the
   * bytes at q, q+1 and q+2 (a byte at or past `limit` counts as matching anything), or `last`.
   */
  using TeddyFn = char const* (*)(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept;

  /** Just after the last '\n' in [floor, p), or `floor` if there is none. */
  using LineStartFn = char const* (*)(char const* p, char const* floor) noexcept;

  struct Backend
  {
    char const* name;              // "scalar", "sse2", "ssse3", "avx2", "avx512", "neon", "sve"
    bool (*runs_here)() noexcept;  // can this CPU (and OS) execute it?
    bool (*preferred)() noexcept;  // pick it automatically when it runs here and nothing better does?
    FindAnyFn find_any;
    TeddyFn teddy;
    LineStartFn line_start;
    bool fast_teddy;               // false: `teddy` is the scalar fallback (no byte shuffle here)
  };

  /** The backend in use: ALF_SIMD=<name> if set and it runs here, otherwise the best one. */
  [[nodiscard]] auto active() -> Backend const&;

  /** Every backend compiled into this binary, best first (scalar last). */
  [[nodiscard]] auto compiled() -> std::span<Backend const* const>;

  /** The compiled backend called `name`, or nullptr. */
  [[nodiscard]] auto by_name(std::string_view name) -> Backend const*;

  /** Make `b` the active backend (tests; it must run here). */
  void use(Backend const& b);

  // The scalar kernels, which other backends use for the few bytes at the end of a range that
  // a vector can't cover.
  auto find_any_scalar(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*;
  auto teddy_scalar(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*;
  auto line_start_scalar(char const* p, char const* floor) noexcept -> char const*;
}
