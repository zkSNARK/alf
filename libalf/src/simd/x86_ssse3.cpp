//
// x86_ssse3.cpp -- Teddy with SSSE3's pshufb: 16 positions per step.  (find_any and line_start
// come from the SSE2 file; SSSE3 adds nothing they need.)
//
// Compiled with -mssse3; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "kernels.h"

#include <tmmintrin.h>

namespace alf::simd
{
  auto teddy_ssse3(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    auto const table = [](unsigned char const* x) { return _mm_load_si128(reinterpret_cast<__m128i const*>(x)); };
    __m128i const lo0 = table(t.lo[0]), lo1 = table(t.lo[1]), lo2 = table(t.lo[2]);
    __m128i const hi0 = table(t.hi[0]), hi1 = table(t.hi[1]), hi2 = table(t.hi[2]);
    __m128i const low4 = _mm_set1_epi8(0x0F), zero = _mm_setzero_si128();
    auto const look = [&](__m128i lo, __m128i hi, char const* x) {
      __m128i const v = _mm_loadu_si128(reinterpret_cast<__m128i const*>(x));
      return _mm_and_si128(_mm_shuffle_epi8(lo, _mm_and_si128(v, low4)),
                           _mm_shuffle_epi8(hi, _mm_and_si128(_mm_srli_epi16(v, 4), low4)));
    };
    while (p < last and limit - p >= 18) {  // three overlapping 16-byte loads: p .. p+17
      __m128i const m = _mm_and_si128(_mm_and_si128(look(lo0, hi0, p), look(lo1, hi1, p + 1)), look(lo2, hi2, p + 2));
      if (unsigned const h = static_cast<unsigned>(~_mm_movemask_epi8(_mm_cmpeq_epi8(m, zero))) & 0xFFFFu; h != 0) {
        char const* q = p + __builtin_ctz(h);
        return q < last ? q : last;
      }
      p += 16;
    }
    return teddy_scalar(t, p, last, limit);
  }
}
