//
// bits.h -- position of the lowest / highest set bit, for turning a compare mask into a lane
// index.  GCC and Clang have builtins for this; MSVC has _BitScanForward / _BitScanReverse.
//
// Every function is `static`: each backend file gets its own private copy, compiled with that
// file's flags, which the linker can't share with other files (see README.md, "the ODR trap").
// All require a non-zero argument.
//

#pragma once

#include <cstdint>

#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#endif

namespace alf::simd::bits
{
#if defined(_MSC_VER) && !defined(__clang__)
  static inline auto lowest32(std::uint32_t x) -> unsigned { unsigned long i; _BitScanForward(&i, x); return static_cast<unsigned>(i); }
  static inline auto highest32(std::uint32_t x) -> unsigned { unsigned long i; _BitScanReverse(&i, x); return static_cast<unsigned>(i); }
  static inline auto lowest64(std::uint64_t x) -> unsigned { unsigned long i; _BitScanForward64(&i, x); return static_cast<unsigned>(i); }
  static inline auto highest64(std::uint64_t x) -> unsigned { unsigned long i; _BitScanReverse64(&i, x); return static_cast<unsigned>(i); }
#else
  static inline auto lowest32(std::uint32_t x) -> unsigned { return static_cast<unsigned>(__builtin_ctz(x)); }
  static inline auto highest32(std::uint32_t x) -> unsigned { return 31u - static_cast<unsigned>(__builtin_clz(x)); }
  static inline auto lowest64(std::uint64_t x) -> unsigned { return static_cast<unsigned>(__builtin_ctzll(x)); }
  static inline auto highest64(std::uint64_t x) -> unsigned { return 63u - static_cast<unsigned>(__builtin_clzll(x)); }
#endif
}
