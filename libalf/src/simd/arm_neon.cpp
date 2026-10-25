//
// arm_neon.cpp -- NEON kernels (every AArch64 CPU has NEON): 16 bytes per vector.
//
// NEON has no "movemask", so the position of the first or last matching lane comes from
// narrowing the 16 compare bytes to 4 bits each (vshrn), giving a 64-bit word whose lowest /
// highest set nibble is the first / last match.
//
// Intrinsics and file-local helpers only (see x86_sse2.cpp on why).
//

#include "kernels.h"

#include <arm_neon.h>
#include <cstdint>

namespace alf::simd
{
  namespace
  {
    inline auto load(char const* p) -> uint8x16_t { return vld1q_u8(reinterpret_cast<std::uint8_t const*>(p)); }

    // 4 bits per lane: lane i is bits 4i..4i+3 (all set if the lane was non-zero)
    inline auto lane_flags(uint8x16_t m) -> std::uint64_t
    {
      uint8x16_t const nonzero = vtstq_u8(m, m);
      return vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(vreinterpretq_u16_u8(nonzero), 4)), 0);
    }
  }

  auto find_any_neon(ByteSet const& set, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    uint8x16_t splat[256];
    for (unsigned i = 0; i < set.count; ++i) {
      splat[i] = vdupq_n_u8(set.bytes[i]);
    }
    auto const hits = [&](char const* x) {
      uint8x16_t const v = load(x);
      uint8x16_t m = vdupq_n_u8(0);
      for (unsigned i = 0; i < set.count; ++i) {
        m = vorrq_u8(m, vceqq_u8(v, splat[i]));
      }
      return m;
    };
    while (last - p >= 64 and limit - p >= 64) {
      uint8x16_t const m = vorrq_u8(vorrq_u8(hits(p), hits(p + 16)), vorrq_u8(hits(p + 32), hits(p + 48)));
      if (vmaxvq_u8(m) != 0) {
        break;
      }
      p += 64;
    }
    while (p < last and limit - p >= 16) {
      uint8x16_t const m = hits(p);
      if (vmaxvq_u8(m) != 0) {
        char const* q = p + (__builtin_ctzll(lane_flags(m)) >> 2);
        return q < last ? q : last;
      }
      p += 16;
    }
    return find_any_scalar(set, p, last, limit);
  }

  auto teddy_neon(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    uint8x16_t const lo0 = vld1q_u8(t.lo[0]), lo1 = vld1q_u8(t.lo[1]), lo2 = vld1q_u8(t.lo[2]);
    uint8x16_t const hi0 = vld1q_u8(t.hi[0]), hi1 = vld1q_u8(t.hi[1]), hi2 = vld1q_u8(t.hi[2]);
    uint8x16_t const low4 = vdupq_n_u8(0x0F);
    auto const look = [&](uint8x16_t lo, uint8x16_t hi, char const* x) {
      uint8x16_t const v = load(x);
      return vandq_u8(vqtbl1q_u8(lo, vandq_u8(v, low4)), vqtbl1q_u8(hi, vshrq_n_u8(v, 4)));
    };
    while (p < last and limit - p >= 18) {  // three overlapping 16-byte loads: p .. p+17
      uint8x16_t const m = vandq_u8(vandq_u8(look(lo0, hi0, p), look(lo1, hi1, p + 1)), look(lo2, hi2, p + 2));
      if (vmaxvq_u8(m) != 0) {
        char const* q = p + (__builtin_ctzll(lane_flags(m)) >> 2);
        return q < last ? q : last;
      }
      p += 16;
    }
    return teddy_scalar(t, p, last, limit);
  }

  auto line_start_neon(char const* p, char const* floor) noexcept -> char const*
  {
    uint8x16_t const nl = vdupq_n_u8('\n');
    while (p - floor >= 16) {
      uint8x16_t const m = vceqq_u8(load(p - 16), nl);
      if (vmaxvq_u8(m) != 0) {
        return p - 16 + ((63 - __builtin_clzll(lane_flags(m))) >> 2) + 1;
      }
      p -= 16;
    }
    return line_start_scalar(p, floor);
  }
}
