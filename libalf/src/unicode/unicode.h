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
#include <string_view>
#include <vector>

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

  // ---- case folding (i'...') -------------------------------------------------------------
  // Strings are compared as characters after Unicode simple case folding, so a match can have a
  // different length in bytes than the string (the Kelvin sign is three bytes, the k it folds to
  // one).  A byte that doesn't start a well-formed character stands for itself: it becomes the
  // pseudo-character RAW + byte, which folds to itself and equals only the same byte.

  inline constexpr char32_t RAW = 0x110000;

  /** Simple case folding of `c` (a pseudo-character folds to itself). */
  [[nodiscard]] inline auto fold(char32_t c) -> char32_t
  {
    if (c < 0x80) {
      return (c >= 'A' and c <= 'Z') ? c - 'A' + 'a' : c;
    }
    std::size_t lo = 0, hi = sizeof fold_table / sizeof fold_table[0];
    while (lo < hi) {
      std::size_t const mid = (lo + hi) / 2;
      if (fold_table[mid].from < c) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    return lo < sizeof fold_table / sizeof fold_table[0] and fold_table[lo].from == c ? fold_table[lo].to : c;
  }

  /** Calls f(c) for every character that folds to `folded` (itself included). */
  template <typename F>
  void each_folding_to(char32_t folded, F&& f)
  {
    std::size_t lo = 0, hi = sizeof orbit_table / sizeof orbit_table[0];
    while (lo < hi) {
      std::size_t const mid = (lo + hi) / 2;
      if (orbit_table[mid].from < folded) {
        lo = mid + 1;
      } else {
        hi = mid;
      }
    }
    if (lo == sizeof orbit_table / sizeof orbit_table[0] or orbit_table[lo].from != folded) {
      f(folded);  // nothing else folds to it
      return;
    }
    for (; lo < sizeof orbit_table / sizeof orbit_table[0] and orbit_table[lo].from == folded; ++lo) {
      f(orbit_table[lo].to);
    }
  }

  /** `c` as UTF-8 (a pseudo-character as its one raw byte), appended to `out`. */
  template <typename String>
  void append_utf8(String& out, char32_t c)
  {
    if (c >= RAW) {
      out += static_cast<char>(c - RAW);
    } else if (c < 0x80) {
      out += static_cast<char>(c);
    } else if (c < 0x800) {
      out += static_cast<char>(0xC0 | (c >> 6));
      out += static_cast<char>(0x80 | (c & 0x3F));
    } else if (c < 0x10000) {
      out += static_cast<char>(0xE0 | (c >> 12));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    } else {
      out += static_cast<char>(0xF0 | (c >> 18));
      out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
      out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
      out += static_cast<char>(0x80 | (c & 0x3F));
    }
  }

  /** The character starting at `at` (before `end`), or a pseudo-character; its length in `len`. */
  [[nodiscard]] inline auto char_at(char const* at, char const* end, std::size_t& len) -> char32_t
  {
    auto const* p = reinterpret_cast<unsigned char const*>(at);
    if (p[0] < 0x80) {
      len = 1;
      return p[0];
    }
    char32_t const c = decode(p, reinterpret_cast<unsigned char const*>(end), len);
    return c == INVALID ? RAW + p[0] : c;
  }

  /**
   * The character ending just before `at` (not looking before `floor`), or a pseudo-character for
   * the byte before `at` if no well-formed character ends exactly there; its length in `len`.
   */
  [[nodiscard]] inline auto char_before(char const* at, char const* floor, std::size_t& len) -> char32_t
  {
    auto const* p = reinterpret_cast<unsigned char const*>(at);
    auto const* f = reinterpret_cast<unsigned char const*>(floor);
    if (p[-1] < 0x80) {
      len = 1;
      return p[-1];
    }
    unsigned char const* q = p - 1;
    while (q > f and p - q < 4 and (*q & 0xC0) == 0x80) {
      --q;
    }
    char32_t const c = decode(q, p, len);
    if (c != INVALID and q + len == p) {
      return c;
    }
    len = 1;
    return RAW + p[-1];
  }

  /** `s` as folded characters (pseudo-characters for bytes that aren't well-formed UTF-8). */
  [[nodiscard]] inline auto folded(std::string_view s) -> std::vector<char32_t>
  {
    std::vector<char32_t> out;
    for (char const* p = s.data(); p < s.data() + s.size();) {
      std::size_t len;
      out.push_back(fold(char_at(p, s.data() + s.size(), len)));
      p += len;
    }
    return out;
  }
}
