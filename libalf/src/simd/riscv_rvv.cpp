//
// riscv_rvv.cpp -- RISC-V Vector (RVV 1.0) kernels, vector-length agnostic like SVE: each step
// handles vsetvl() bytes (16 on a 128-bit implementation, more on wider ones).  Loads take an
// exact element count, so lanes past the end are never read.
//
// Teddy's lookup is vrgather: a 16-entry table in a vector register, indexed by each byte's
// low or high 4 bits (indices < 16, so only the table's first 16 elements are ever read).
// The first match is vfirst; the last '\n' is a masked maximum over the lane indices.
//
// Compiled with -march=rv64gcv; intrinsics and file-local helpers only (see x86_sse2.cpp).
//

#include "kernels.h"

#include <cstdint>
#include <riscv_vector.h>

namespace alf::simd
{
  auto find_any_rvv(ByteSet const& set, char const* p, char const* last, char const*) noexcept -> char const*
  {
    if (set.count == 0) {
      return last;
    }
    auto const* u = reinterpret_cast<std::uint8_t const*>(p);
    std::size_t const n = static_cast<std::size_t>(last - p);
    for (std::size_t i = 0; i < n;) {
      std::size_t const vl = __riscv_vsetvl_e8m1(n - i);
      vuint8m1_t const v = __riscv_vle8_v_u8m1(u + i, vl);
      vbool8_t m = __riscv_vmseq_vx_u8m1_b8(v, set.bytes[0], vl);
      for (unsigned k = 1; k < set.count; ++k) {
        m = __riscv_vmor_mm_b8(m, __riscv_vmseq_vx_u8m1_b8(v, set.bytes[k], vl), vl);
      }
      if (long const first = __riscv_vfirst_m_b8(m, vl); first >= 0) {
        return p + i + static_cast<std::size_t>(first);
      }
      i += vl;
    }
    return last;
  }

  auto teddy_rvv(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    std::size_t const tvl = __riscv_vsetvl_e8m1(16);  // RVV 1.0 'V' guarantees at least 16 bytes
    vuint8m1_t const lo0 = __riscv_vle8_v_u8m1(t.lo[0], tvl), lo1 = __riscv_vle8_v_u8m1(t.lo[1], tvl), lo2 = __riscv_vle8_v_u8m1(t.lo[2], tvl);
    vuint8m1_t const hi0 = __riscv_vle8_v_u8m1(t.hi[0], tvl), hi1 = __riscv_vle8_v_u8m1(t.hi[1], tvl), hi2 = __riscv_vle8_v_u8m1(t.hi[2], tvl);
    auto const* u = reinterpret_cast<std::uint8_t const*>(p);
    // positions whose three fingerprint bytes are all readable
    std::ptrdiff_t full = limit - p - 2;
    if (full > last - p) {
      full = last - p;
    }
    if (full < 0) {
      full = 0;
    }
    std::size_t const n = static_cast<std::size_t>(full);
    for (std::size_t i = 0; i < n;) {
      std::size_t const vl = __riscv_vsetvl_e8m1(n - i);
      auto const look = [&](vuint8m1_t lo, vuint8m1_t hi, std::uint8_t const* x) {
        vuint8m1_t const v = __riscv_vle8_v_u8m1(x, vl);
        return __riscv_vand_vv_u8m1(__riscv_vrgather_vv_u8m1(lo, __riscv_vand_vx_u8m1(v, 0x0F, vl), vl),
                                    __riscv_vrgather_vv_u8m1(hi, __riscv_vsrl_vx_u8m1(v, 4, vl), vl), vl);
      };
      vuint8m1_t const m = __riscv_vand_vv_u8m1(__riscv_vand_vv_u8m1(look(lo0, hi0, u + i), look(lo1, hi1, u + i + 1), vl),
                                                look(lo2, hi2, u + i + 2), vl);
      if (long const first = __riscv_vfirst_m_b8(__riscv_vmsne_vx_u8m1_b8(m, 0, vl), vl); first >= 0) {
        return p + i + static_cast<std::size_t>(first);
      }
      i += vl;
    }
    return teddy_scalar(t, p + n, last, limit);
  }

  auto line_start_rvv(char const* p, char const* floor) noexcept -> char const*
  {
    // walk back one vector at a time; within a vector, the last '\n' is the largest lane index
    // among the matches (indices fit in 16 bits for any vector length)
    while (p > floor) {
      std::size_t const vl = __riscv_vsetvl_e8m1(static_cast<std::size_t>(p - floor));
      auto const* u = reinterpret_cast<std::uint8_t const*>(p - vl);
      vbool8_t const nl = __riscv_vmseq_vx_u8m1_b8(__riscv_vle8_v_u8m1(u, vl), '\n', vl);
      if (__riscv_vfirst_m_b8(nl, vl) >= 0) {
        vuint16m2_t const lane = __riscv_vid_v_u16m2(vl);
        vuint16m1_t const zero = __riscv_vmv_v_x_u16m1(0, 1);
        std::uint16_t const at = __riscv_vmv_x_s_u16m1_u16(__riscv_vredmaxu_vs_u16m2_u16m1_m(nl, lane, zero, vl));
        return p - vl + at + 1;
      }
      p -= vl;
    }
    return floor;
  }
}
