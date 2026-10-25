//
// scalar.cpp -- the reference kernels.  Every other backend must return exactly what these
// return; tests/test_simd.cpp checks that on random inputs.  Also used by the other backends
// for the last few bytes of a range, where a whole vector can't be loaded.
//

#include "simd.h"

namespace alf::simd
{
  auto find_any_scalar(ByteSet const& set, char const* p, char const* last, char const*) noexcept -> char const*
  {
    for (; p < last; ++p) {
      if (set.member[static_cast<unsigned char>(*p)]) {
        return p;
      }
    }
    return last;
  }

  auto teddy_scalar(TeddyTables const& t, char const* p, char const* last, char const* limit) noexcept -> char const*
  {
    for (; p < last; ++p) {
      unsigned m = 0xFF;
      for (int k = 0; k < 3 and p + k < limit; ++k) {
        auto const b = static_cast<unsigned char>(p[k]);
        m &= static_cast<unsigned>(t.lo[k][b & 15u] & t.hi[k][b >> 4]);
      }
      if (m != 0) {
        return p;
      }
    }
    return last;
  }

  auto line_start_scalar(char const* p, char const* floor) noexcept -> char const*
  {
    while (p > floor and p[-1] != '\n') {
      --p;
    }
    return p;
  }
}
