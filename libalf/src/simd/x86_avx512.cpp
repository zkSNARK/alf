//
// x86_avx512.cpp -- AVX-512 (F + BW) kernels: 64 bytes per step, with compare results as 64-bit
// masks.  vpshufb works within each 128-bit quarter, so the Teddy tables are broadcast to all
// four.
//
// Not chosen automatically yet: on some Intel CPUs, 512-bit instructions lower the clock speed
// for everything else running on that core, and that hasn't been measured for alf.  ALF_SIMD=avx512
// selects it.
//
// Compiled with -mavx512f -mavx512bw; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "kernels.h"

#include <immintrin.h>

namespace alf::simd
{
  namespace
  {
    inline auto load(char const* p) -> __m512i { return _mm512_loadu_si512(p); }
  }

  auto find_any_avx512(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    __m512i splat[256];
    for (unsigned i = 0; i < set.count; ++i) {
      splat[i] = _mm512_set1_epi8(static_cast<char>(set.bytes[i]));
    }
    while (p < last and limit - p >= 64) {
      __m512i const v = load(p);
      __mmask64 m = 0;
      for (unsigned i = 0; i < set.count; ++i) {
        m |= _mm512_cmpeq_epi8_mask(v, splat[i]);
      }
      if (m != 0) {
        char const* q = p + __builtin_ctzll(m);
        return q < last ? q : last;
      }
      p += 64;
    }
    return find_any_scalar(set, p, last, limit);
  }

  auto teddy_avx512(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    auto const table = [](unsigned char const* x) {
      return _mm512_broadcast_i32x4(_mm_load_si128(reinterpret_cast<__m128i const*>(x)));
    };
    __m512i const lo0 = table(t.lo[0]), lo1 = table(t.lo[1]), lo2 = table(t.lo[2]);
    __m512i const hi0 = table(t.hi[0]), hi1 = table(t.hi[1]), hi2 = table(t.hi[2]);
    __m512i const low4 = _mm512_set1_epi8(0x0F);
    auto const look = [&](__m512i lo, __m512i hi, char const* x) {
      __m512i const v = load(x);
      return _mm512_and_si512(_mm512_shuffle_epi8(lo, _mm512_and_si512(v, low4)),
                              _mm512_shuffle_epi8(hi, _mm512_and_si512(_mm512_srli_epi16(v, 4), low4)));
    };
    while (p < last and limit - p >= 66) {  // three overlapping 64-byte loads: p .. p+65
      __m512i const m = _mm512_and_si512(_mm512_and_si512(look(lo0, hi0, p), look(lo1, hi1, p + 1)), look(lo2, hi2, p + 2));
      if (__mmask64 const h = _mm512_test_epi8_mask(m, m); h != 0) {
        char const* q = p + __builtin_ctzll(h);
        return q < last ? q : last;
      }
      p += 64;
    }
    return teddy_scalar(t, p, last, limit);
  }

  auto line_start_avx512(char const* p, char const* floor) noexcept -> char const*
  {
    __m512i const nl = _mm512_set1_epi8('\n');
    while (p - floor >= 64) {
      if (__mmask64 const h = _mm512_cmpeq_epi8_mask(load(p - 64), nl); h != 0) {
        return p - 64 + (63 - __builtin_clzll(h)) + 1;
      }
      p -= 64;
    }
    return line_start_scalar(p, floor);
  }
}
