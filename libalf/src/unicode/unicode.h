//
// unicode.h -- the little Unicode alf needs: is the character just before or just after a match
// a word character?  (w'...' matches only where neither is, as ripgrep's -w does.)
//
// The input is treated as UTF-8 where it is valid.  A byte that doesn't start a valid UTF-8
// character (or a stray continuation byte) is not a word character, so it counts as a boundary,
// as in ripgrep.  ASCII bytes take a fast path that never touches the table.
//

#pragma once

#include "unicode_tables.h"

#include <cstddef>

namespace alf::unicode
{
  inline constexpr char32_t INVALID = 0xFFFFFFFF;

  [[nodiscard]] inline auto ascii_word(unsigned char b) -> bool
  {
    return (b >= 'a' and b <= 'z') or (b >= 'A' and b <= 'Z') or (b >= '0' and b <= '9') or b == '_';
  }

  [[nodiscard]] inline auto is_word(char32_t c) -> bool
  {
    if (c < 0x80) {
      return ascii_word(static_cast<unsigned char>(c));
    }
    std::size_t lo = 0, hi = sizeof word_ranges / sizeof word_ranges[0];
    while (lo < hi) {
      std::size_t const mid = (lo + hi) / 2;
      if (c < word_ranges[mid].lo) {
        hi = mid;
      } else if (c > word_ranges[mid].hi) {
        lo = mid + 1;
      } else {
        return true;
      }
    }
    return false;
  }

  // The character encoded in [p, end) starting at p (a lead byte), and how many bytes it takes;
  // INVALID (and length 1) if those bytes aren't a well-formed UTF-8 character.
  [[nodiscard]] inline auto decode(unsigned char const* p, unsigned char const* end, std::size_t& len) -> char32_t
  {
    len = 1;
    unsigned char const b = p[0];
    std::size_t n;
    char32_t c, min;
    if (b < 0x80) {
      return b;
    } else if ((b & 0xE0) == 0xC0) {
      n = 2, c = b & 0x1F, min = 0x80;
    } else if ((b & 0xF0) == 0xE0) {
      n = 3, c = b & 0x0F, min = 0x800;
    } else if ((b & 0xF8) == 0xF0) {
      n = 4, c = b & 0x07, min = 0x10000;
    } else {
      return INVALID;
    }
    if (static_cast<std::size_t>(end - p) < n) {
      return INVALID;
    }
    for (std::size_t k = 1; k < n; ++k) {
      if ((p[k] & 0xC0) != 0x80) {
        return INVALID;
      }
      c = (c << 6) | (p[k] & 0x3F);
    }
    if (c < min or c > 0x10FFFF or (c >= 0xD800 and c <= 0xDFFF)) {
      return INVALID;  // overlong, out of range, or a surrogate
    }
    len = n;
    return c;
  }

  /** Is the character that ends just before `at` (not looking before `floor`) a word character? */
  [[nodiscard]] inline auto word_before(char const* at, char const* floor) -> bool
  {
    if (at == floor) {
      return false;
    }
    auto const* p = reinterpret_cast<unsigned char const*>(at);
    auto const* f = reinterpret_cast<unsigned char const*>(floor);
    if (p[-1] < 0x80) {
      return ascii_word(p[-1]);
    }
    // back over at most three continuation bytes to the lead byte
    unsigned char const* q = p - 1;
    while (q > f and p - q < 4 and (*q & 0xC0) == 0x80) {
      --q;
    }
    std::size_t len;
    char32_t const c = decode(q, p, len);
    return c != INVALID and q + len == p and is_word(c);
  }

  /** Is the character that starts at `at` (before `end`) a word character? */
  [[nodiscard]] inline auto word_after(char const* at, char const* end) -> bool
  {
    if (at == end) {
      return false;
    }
    auto const* p = reinterpret_cast<unsigned char const*>(at);
    if (p[0] < 0x80) {
      return ascii_word(p[0]);
    }
    std::size_t len;
    char32_t const c = decode(p, reinterpret_cast<unsigned char const*>(end), len);
    return c != INVALID and is_word(c);
  }
}
