//
// x86_avx2.cpp -- AVX2 kernels: 32 bytes per step.  vpshufb works within each 128-bit half,
// so the 16-entry Teddy tables are broadcast to both halves.
//
// Compiled with -mavx2; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "bits.h"
#include "kernels.h"

#include <immintrin.h>

namespace alf::simd
{
  namespace
  {
    inline auto load(char const* p) -> __m256i { return _mm256_loadu_si256(reinterpret_cast<__m256i const*>(p)); }
  }

  auto find_any_avx2(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    __m256i splat[256];
    for (unsigned i = 0; i < set.count; ++i) {
      splat[i] = _mm256_set1_epi8(static_cast<char>(set.bytes[i]));
    }
    auto const hits = [&](char const* x) {
      __m256i const v = load(x);
      __m256i m = _mm256_setzero_si256();
      for (unsigned i = 0; i < set.count; ++i) {
        m = _mm256_or_si256(m, _mm256_cmpeq_epi8(v, splat[i]));
      }
      return static_cast<unsigned>(_mm256_movemask_epi8(m));
    };
    while (last - p >= 128 and limit - p >= 128) {
      if ((hits(p) | hits(p + 32) | hits(p + 64) | hits(p + 96)) != 0) {
        break;
      }
      p += 128;
    }
    while (p < last and limit - p >= 32) {
      if (unsigned const h = hits(p); h != 0) {
        char const* q = p + bits::lowest32(h);
        return q < last ? q : last;
      }
      p += 32;
    }
    return find_any_scalar(set, p, last, limit);
  }

  auto teddy_avx2(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    auto const table = [](unsigned char const* x) {
      return _mm256_broadcastsi128_si256(_mm_load_si128(reinterpret_cast<__m128i const*>(x)));
    };
    __m256i const lo0 = table(t.lo[0]), lo1 = table(t.lo[1]), lo2 = table(t.lo[2]);
    __m256i const hi0 = table(t.hi[0]), hi1 = table(t.hi[1]), hi2 = table(t.hi[2]);
    __m256i const low4 = _mm256_set1_epi8(0x0F), zero = _mm256_setzero_si256();
    auto const look = [&](__m256i lo, __m256i hi, char const* x) {
      __m256i const v = load(x);
      return _mm256_and_si256(_mm256_shuffle_epi8(lo, _mm256_and_si256(v, low4)),
                              _mm256_shuffle_epi8(hi, _mm256_and_si256(_mm256_srli_epi16(v, 4), low4)));
    };
    while (p < last and limit - p >= 34) {  // three overlapping 32-byte loads: p .. p+33
      __m256i const m = _mm256_and_si256(_mm256_and_si256(look(lo0, hi0, p), look(lo1, hi1, p + 1)), look(lo2, hi2, p + 2));
      if (unsigned const h = ~static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(m, zero))); h != 0) {
        char const* q = p + bits::lowest32(h);
        return q < last ? q : last;
      }
      p += 32;
    }
    return teddy_scalar(t, p, last, limit);
  }

  auto line_start_avx2(char const* p, char const* floor) noexcept -> char const*
  {
    __m256i const nl = _mm256_set1_epi8('\n');
    while (p - floor >= 32) {
      if (unsigned const h = static_cast<unsigned>(_mm256_movemask_epi8(_mm256_cmpeq_epi8(load(p - 32), nl))); h != 0) {
        return p - 32 + bits::highest32(h) + 1;
      }
      p -= 32;
    }
    return line_start_scalar(p, floor);
  }
}
