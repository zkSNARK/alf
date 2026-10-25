//
// x86_sse2.cpp -- SSE2 kernels (every x86-64 CPU has SSE2).  SSE2 has no byte shuffle, so this
// backend has no fast Teddy; see x86_ssse3.cpp.
//
// Compiled with -msse2.  Uses only intrinsics and file-local helpers: no standard-library
// templates, which could leak code compiled for this file's flags into the rest of the program.
//

#include "bits.h"
#include "kernels.h"

#include <emmintrin.h>

namespace alf::simd
{
  namespace
  {
    inline auto load(char const* p) -> __m128i { return _mm_loadu_si128(reinterpret_cast<__m128i const*>(p)); }
  }

  auto find_any_sse2(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    __m128i splat[256];
    for (unsigned i = 0; i < set.count; ++i) {
      splat[i] = _mm_set1_epi8(static_cast<char>(set.bytes[i]));
    }
    auto const hits = [&](char const* x) {
      __m128i const v = load(x);
      __m128i m = _mm_setzero_si128();
      for (unsigned i = 0; i < set.count; ++i) {
        m = _mm_or_si128(m, _mm_cmpeq_epi8(v, splat[i]));
      }
      return static_cast<unsigned>(_mm_movemask_epi8(m));
    };
    while (last - p >= 64 and limit - p >= 64) {
      if ((hits(p) | hits(p + 16) | hits(p + 32) | hits(p + 48)) != 0) {
        break;
      }
      p += 64;
    }
    while (p < last and limit - p >= 16) {
      if (unsigned const h = hits(p); h != 0) {
        char const* q = p + bits::lowest32(h);
        return q < last ? q : last;
      }
      p += 16;
    }
    return find_any_scalar(set, p, last, limit);
  }

  auto line_start_sse2(char const* p, char const* floor) noexcept -> char const*
  {
    __m128i const nl = _mm_set1_epi8('\n');
    while (p - floor >= 16) {
      if (unsigned const h = static_cast<unsigned>(_mm_movemask_epi8(_mm_cmpeq_epi8(load(p - 16), nl))); h != 0) {
        return p - 16 + bits::highest32(h) + 1;  // just after the last '\n' in these 16
      }
      p -= 16;
    }
    return line_start_scalar(p, floor);
  }
}
