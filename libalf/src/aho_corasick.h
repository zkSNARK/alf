//
// aho_corasick.h -- a DFA that finds every occurrence of many strings in one pass: one table
// lookup per byte, however many strings there are.
//
// Teddy (program.cpp) is faster for a few dozen strings, but its cost grows with the list: its 8
// groups fill up, so it stops more often for nothing and compares more strings at each stop.  A
// DFA's cost per byte stays the same; only its table grows, which matters once it no longer fits
// in cache.  This is what ripgrep switches to for long lists too.
//
// Layout (as in ripgrep's aho-corasick crate):
//  - Byte classes: every byte that occurs in some string gets its own class, all the others share
//    class 0, so a row of the table has one entry per class rather than 256.
//  - State ids are row offsets (index * classes), so a step is one load: next[state + class[b]].
//  - States in which some string ends are numbered last, so "did a string just end?" is one
//    comparison with first_match.
//

#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace alf::detail
{
  class AhoCorasick
  {
  public:
    /**
     * `strings` must be non-empty and none of them empty.  String i is reported as index i.
     * With `fold`, ASCII letters are matched in either case (every string, so a caller with
     * case-sensitive strings among them checks those matches itself).
     */
    explicit AhoCorasick(std::vector<std::string_view> const& strings, bool fold = false);

    static constexpr std::uint32_t start = 0;

    /** The state after reading byte `b` in state `s`. */
    [[nodiscard]] auto step(std::uint32_t s, unsigned char b) const -> std::uint32_t { return m_next[s + m_class[b]]; }

    /** Does some string end at the byte that led to state `s`? */
    [[nodiscard]] auto matches(std::uint32_t s) const -> bool { return s >= m_first_match; }

    /** The strings that end at the byte that led to match state `s` (every one, overlaps included). */
    [[nodiscard]] auto ending(std::uint32_t s) const -> std::span<std::uint32_t const>
    {
      std::uint32_t const k = (s - m_first_match) / m_classes;
      return { m_out.data() + m_out_at[k], m_out_at[k + 1] - m_out_at[k] };
    }

    [[nodiscard]] auto states() const -> std::size_t { return m_next.size() / m_classes; }

  private:
    std::array<std::uint32_t, 256> m_class{};  // byte -> class
    std::uint32_t m_classes{ 1 };
    std::vector<std::uint32_t> m_next;     // states x classes, entries are state ids (row offsets)
    std::uint32_t m_first_match{ 0 };      // id of the first state with a match
    std::vector<std::uint32_t> m_out_at;   // match state k's strings: m_out[m_out_at[k] .. m_out_at[k + 1])
    std::vector<std::uint32_t> m_out;
  };
}
