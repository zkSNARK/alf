//
// power_vsx.cpp -- POWER8+ VSX kernels (little-endian ppc64le, where POWER8 and VSX are the
// baseline): 16 bytes per step.  vec_perm is the Teddy table lookup (indices < 16 select from
// the first operand).  POWER8 has no cheap "which lane matched", so once a vector says something
// is in these 16 bytes, the exact position comes from the scalar kernel on just those bytes,
// which also guarantees the same answer as the reference.
//
// Compiled with -mcpu=power8 -mvsx; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "kernels.h"

#include <altivec.h>

namespace alf::simd
{
  namespace
  {
    using V = __vector unsigned char;

    inline auto load(char const* p) -> V { return vec_xl(0, reinterpret_cast<unsigned char const*>(p)); }

    inline auto least(char const* a, char const* b) -> char const* { return a < b ? a : b; }
  }

  auto find_any_vsx(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    V splat[256];
    for (unsigned i = 0; i < set.count; ++i) {
      splat[i] = vec_splats(set.bytes[i]);
    }
    V const zero = vec_splats(static_cast<unsigned char>(0));
    while (p < last and limit - p >= 16) {
      V const v = load(p);
      V m = zero;
      for (unsigned i = 0; i < set.count; ++i) {
        m = vec_or(m, reinterpret_cast<V>(vec_cmpeq(v, splat[i])));
      }
      if (vec_any_ne(m, zero)) {
        char const* e = least(p + 16, last);
        char const* q = find_any_scalar(set, p, e, limit);
        return q == e ? last : q;  // the match was at or past `last`
      }
      p += 16;
    }
    return find_any_scalar(set, p, last, limit);
  }

  auto teddy_vsx(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    auto const table = [](unsigned char const* x) { return vec_xl(0, x); };
    V const lo0 = table(t.lo[0]), lo1 = table(t.lo[1]), lo2 = table(t.lo[2]);
    V const hi0 = table(t.hi[0]), hi1 = table(t.hi[1]), hi2 = table(t.hi[2]);
    V const low4 = vec_splats(static_cast<unsigned char>(0x0F)), four = vec_splats(static_cast<unsigned char>(4));
    V const zero = vec_splats(static_cast<unsigned char>(0));
    auto const look = [&](V lo, V hi, char const* x) {
      V const v = load(x);
      return vec_and(vec_perm(lo, lo, vec_and(v, low4)), vec_perm(hi, hi, vec_sr(v, four)));
    };
    while (p < last and limit - p >= 18) {  // three overlapping 16-byte loads: p .. p+17
      V const m = vec_and(vec_and(look(lo0, hi0, p), look(lo1, hi1, p + 1)), look(lo2, hi2, p + 2));
      if (vec_any_ne(m, zero)) {
        char const* e = least(p + 16, last);
        char const* q = teddy_scalar(t, p, e, limit);
        return q == e ? last : q;
      }
      p += 16;
    }
    return teddy_scalar(t, p, last, limit);
  }

  auto line_start_vsx(char const* p, char const* floor) noexcept -> char const*
  {
    V const nl = vec_splats(static_cast<unsigned char>('\n'));
    while (p - floor >= 16) {
      if (vec_any_eq(load(p - 16), nl)) {
        return line_start_scalar(p, p - 16);  // the '\n' is in these 16 bytes
      }
      p -= 16;
    }
    return line_start_scalar(p, floor);
  }

  auto find_pair_vsx(BytePair const& pair, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    V const a = vec_splats(pair.a), b = vec_splats(pair.b);
    V const zero = vec_splats(static_cast<unsigned char>(0));
    std::ptrdiff_t const off = pair.offset;
    auto const hits = [&](char const* x) { return vec_and(reinterpret_cast<V>(vec_cmpeq(load(x), a)), reinterpret_cast<V>(vec_cmpeq(load(x + off), b))); };
    while (last - p >= 64 and limit - p - off >= 64) {  // 64 bytes per step while nothing is found
      if (vec_any_ne(vec_or(vec_or(hits(p), hits(p + 16)), vec_or(hits(p + 32), hits(p + 48))), zero)) {
        break;
      }
      p += 64;
    }
    while (p < last and limit - p - off >= 16) {
      V const m = hits(p);
      if (vec_any_ne(m, zero)) {
        char const* e = least(p + 16, last);
        char const* q = find_pair_scalar(pair, p, e, limit);
        return q == e ? last : q;
      }
      p += 16;
    }
    return find_pair_scalar(pair, p, last, limit);
  }
}
