//
// arm_sve.cpp -- SVE kernels, written vector-length-agnostic: the same code runs 16 bytes per
// step on a 128-bit implementation and 32 or 64 on wider ones (svcntb() bytes per vector).
// Loads are predicated, so lanes past the end are never read and no `limit` slack is needed
// except for Teddy's three overlapping loads.
//
// Teddy's 16-entry tables sit in the low 16 bytes of a vector; svtbl with indices 0..15 only
// ever reads those, whatever the vector length.  line_start uses the NEON kernel (every SVE CPU
// also has NEON).
//
// Compiled with -march=armv8-a+sve; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "kernels.h"

#include <arm_sve.h>
#include <cstdint>

namespace alf::simd
{
  auto sve_vector_bytes() noexcept -> unsigned { return static_cast<unsigned>(svcntb()); }

  auto find_any_sve(ByteSet const& set, char const* p, char const* last, char const*) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    auto const* u = reinterpret_cast<std::uint8_t const*>(p);
    std::int64_t const n = last - p;
    for (std::int64_t i = 0; i < n; i += static_cast<std::int64_t>(svcntb())) {
      svbool_t const pg = svwhilelt_b8_s64(i, n);
      svuint8_t const v = svld1_u8(pg, u + i);
      svbool_t m = svpfalse_b();
      for (unsigned k = 0; k < set.count; ++k) {
        m = svorr_b_z(pg, m, svcmpeq_n_u8(pg, v, set.bytes[k]));
      }
      if (svptest_any(pg, m)) {
        return p + i + static_cast<std::int64_t>(svcntp_b8(pg, svbrkb_b_z(pg, m)));  // lanes before the first match
      }
    }
    return last;
  }

  auto teddy_sve(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    svbool_t const first16 = svwhilelt_b8_s32(0, 16);
    svuint8_t const lo0 = svld1_u8(first16, t.lo[0]), lo1 = svld1_u8(first16, t.lo[1]), lo2 = svld1_u8(first16, t.lo[2]);
    svuint8_t const hi0 = svld1_u8(first16, t.hi[0]), hi1 = svld1_u8(first16, t.hi[1]), hi2 = svld1_u8(first16, t.hi[2]);
    auto const* u = reinterpret_cast<std::uint8_t const*>(p);
    // positions whose three fingerprint bytes are all readable
    std::int64_t full = limit - p - 2;
    if (full > last - p) {
      full = last - p;
    }
    if (full < 0) {
      full = 0;
    }
    for (std::int64_t i = 0; i < full; i += static_cast<std::int64_t>(svcntb())) {
      svbool_t const pg = svwhilelt_b8_s64(i, full);
      auto const look = [&](svuint8_t lo, svuint8_t hi, std::uint8_t const* x) {
        svuint8_t const v = svld1_u8(pg, x);
        return svand_u8_x(pg, svtbl_u8(lo, svand_n_u8_x(pg, v, 0x0F)), svtbl_u8(hi, svlsr_n_u8_x(pg, v, 4)));
      };
      svuint8_t const m = svand_u8_x(pg, svand_u8_x(pg, look(lo0, hi0, u + i), look(lo1, hi1, u + i + 1)), look(lo2, hi2, u + i + 2));
      svbool_t const hit = svcmpne_n_u8(pg, m, 0);
      if (svptest_any(pg, hit)) {
        return p + i + static_cast<std::int64_t>(svcntp_b8(pg, svbrkb_b_z(pg, hit)));
      }
    }
    return teddy_scalar(t, p + full, last, limit);
  }
}
