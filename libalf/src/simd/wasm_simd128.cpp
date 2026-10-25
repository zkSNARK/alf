//
// wasm_simd128.cpp -- WebAssembly 128-bit SIMD kernels: 16 bytes per step.  i8x16.swizzle is
// Teddy's table lookup (indices < 16 select from the table) and i8x16.bitmask is the
// "which lanes matched" mask, like x86's movemask.
//
// WebAssembly can't test for SIMD at run time: a module that contains SIMD instructions simply
// needs an engine that supports them (every current browser and runtime does).  So this backend
// is chosen at build time: it's compiled in when building for WebAssembly, and it then always
// runs.
//
// Compiled with -msimd128; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "kernels.h"

#include <wasm_simd128.h>

namespace alf::simd
{
  namespace
  {
    inline auto load(char const* p) -> v128_t { return wasm_v128_load(p); }
  }

  auto find_any_wasm(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    v128_t splat[256];
    for (unsigned i = 0; i < set.count; ++i) {
      splat[i] = wasm_i8x16_splat(static_cast<signed char>(set.bytes[i]));
    }
    while (p < last and limit - p >= 16) {
      v128_t const v = load(p);
      v128_t m = wasm_i64x2_const(0, 0);
      for (unsigned i = 0; i < set.count; ++i) {
        m = wasm_v128_or(m, wasm_i8x16_eq(v, splat[i]));
      }
      if (unsigned const h = wasm_i8x16_bitmask(m); h != 0) {
        char const* q = p + __builtin_ctz(h);
        return q < last ? q : last;
      }
      p += 16;
    }
    return find_any_scalar(set, p, last, limit);
  }

  auto teddy_wasm(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    v128_t const lo0 = wasm_v128_load(t.lo[0]), lo1 = wasm_v128_load(t.lo[1]), lo2 = wasm_v128_load(t.lo[2]);
    v128_t const hi0 = wasm_v128_load(t.hi[0]), hi1 = wasm_v128_load(t.hi[1]), hi2 = wasm_v128_load(t.hi[2]);
    v128_t const low4 = wasm_i8x16_splat(0x0F);
    auto const look = [&](v128_t lo, v128_t hi, char const* x) {
      v128_t const v = load(x);
      return wasm_v128_and(wasm_i8x16_swizzle(lo, wasm_v128_and(v, low4)), wasm_i8x16_swizzle(hi, wasm_u8x16_shr(v, 4)));
    };
    while (p < last and limit - p >= 18) {  // three overlapping 16-byte loads: p .. p+17
      v128_t const m = wasm_v128_and(wasm_v128_and(look(lo0, hi0, p), look(lo1, hi1, p + 1)), look(lo2, hi2, p + 2));
      if (unsigned const h = wasm_i8x16_bitmask(wasm_i8x16_ne(m, wasm_i64x2_const(0, 0))); h != 0) {
        char const* q = p + __builtin_ctz(h);
        return q < last ? q : last;
      }
      p += 16;
    }
    return teddy_scalar(t, p, last, limit);
  }

  auto line_start_wasm(char const* p, char const* floor) noexcept -> char const*
  {
    v128_t const nl = wasm_i8x16_splat('\n');
    while (p - floor >= 16) {
      if (unsigned const h = wasm_i8x16_bitmask(wasm_i8x16_eq(load(p - 16), nl)); h != 0) {
        return p - 16 + (31 - __builtin_clz(h)) + 1;
      }
      p -= 16;
    }
    return line_start_scalar(p, floor);
  }
}
