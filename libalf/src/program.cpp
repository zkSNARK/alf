//
// program.cpp
//

#include "program.h"

#include "aho_corasick.h"
#include "unicode/unicode.h"
#include "simd.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <optional>
#include <stdexcept>
#include <thread>
#include <utility>

namespace
{
  // Below this much data a second thread costs more than it saves.
  constexpr std::size_t MIN_BYTES_PER_THREAD = 1 << 20;

  // How much input to sample when choosing between searching for the strings and reading every
  // line (the cut-over share of lines with a hit is filter()'s max_hit_fraction).
  constexpr std::size_t SAMPLE_BYTES = 1 << 16;

  // Kept views averaging less than this many bytes are packed into one buffer before being
  // handed over (by each worker thread in parallel, or per batch on one thread).
  constexpr std::size_t PACK_BELOW = 16 << 10;

  auto line_end(char const* p, char const* end) -> char const*
  {
    auto nl = static_cast<char const*>(std::memchr(p, '\n', static_cast<std::size_t>(end - p)));
    return nl ? nl : end;
  }

  // i'...' and w'...': ASCII case folding and word bytes, the same rules as passes_filters.
  inline auto lower(unsigned char b) -> unsigned char { return (b >= 'A' and b <= 'Z') ? static_cast<unsigned char>(b - 'A' + 'a') : b; }
  inline auto lower(char c) -> unsigned char { return lower(static_cast<unsigned char>(c)); }
  inline auto upper(unsigned char b) -> unsigned char { return (b >= 'a' and b <= 'z') ? static_cast<unsigned char>(b - 'a' + 'A') : b; }

  // Do the `len` bytes at `start` stand alone as a word: is neither the character before them nor
  // the one after them a word character (Unicode, decoded as UTF-8; see unicode/unicode.h)?
  // `first` and `last` bound the line (or a stretch that starts and ends at line breaks): nothing
  // before `first` or from `last` on counts.
  inline auto whole_word(char const* start, std::size_t len, char const* first, char const* last) -> bool
  {
    return !alf::unicode::word_before(start, first) and !alf::unicode::word_after(start + len, last);
  }

  inline auto same_text(char const* p, std::string_view s, bool nocase) -> bool
  {
    if (!nocase) {
      return std::memcmp(p, s.data(), s.size()) == 0;
    }
    for (std::size_t k = 0; k < s.size(); ++k) {
      if (lower(p[k]) != lower(s[k])) {
        return false;
      }
    }
    return true;
  }

  // Does `line` contain `s` (with i'...' / w'...' rules)?  Every occurrence is tried: in
  // "foobar foo", w'foo' fails at the first and matches at the second.
  auto contains(std::string_view line, std::string_view s, bool nocase, bool word) -> bool
  {
    if (!nocase and !word) {
      return line.find(s) != std::string_view::npos;
    }
    char const* const b = line.data();
    char const* const e = b + line.size();
    auto const eq = [nocase](char x, char y) { return nocase ? lower(x) == lower(y) : x == y; };
    for (char const* at = b; static_cast<std::size_t>(e - at) >= s.size(); ++at) {
      at = std::search(at, e, s.begin(), s.end(), eq);
      if (at == e) {
        return false;
      }
      if (!word or whole_word(at, s.size(), b, e)) {
        return true;
      }
    }
    return false;
  }

  // A string to search for, and which of its bytes is rarest in the input: memchr finds that
  // byte (it checks many bytes per instruction), then memcmp checks the rest of the string.
  // This is the same trick grep uses, and on this kind of data it is several times faster than
  // memmem or Boyer-Moore-Horspool.  For an i'...' string a letter's frequency is both cases'.
  struct Needle
  {
    std::string_view text;
    std::size_t rare;
    bool nocase{ false };
    bool word{ false };
  };

  auto make_needles(std::vector<alf::Literal> const& literals, std::string_view sample) -> std::vector<Needle>
  {
    std::size_t freq[256] = {};
    for (unsigned char c : sample) {
      ++freq[c];
    }
    std::vector<Needle> needles;
    for (auto const& lit : literals) {
      auto const f = [&](char c) {
        auto const b = static_cast<unsigned char>(c);
        return lit.nocase and lower(b) != upper(b) ? freq[lower(b)] + freq[upper(b)] : freq[b];
      };
      std::size_t rare = 0;
      for (std::size_t i = 1; i < lit.text.size(); ++i) {
        if (f(lit.text[i]) < f(lit.text[rare])) {
          rare = i;
        }
      }
      needles.push_back({ lit.text, rare, lit.nocase, lit.word });
    }
    return needles;
  }

  // Start of the first occurrence of `n` in [p, last), or `last`.
  auto find(Needle const& n, char const* p, char const* last) -> char const*
  {
    if (static_cast<std::size_t>(last - p) < n.text.size()) {
      return last;
    }
    char const key = n.text[n.rare];
    char const* q = p + n.rare;
    char const* const q_end = last - (n.text.size() - n.rare - 1);  // a hit must fit before `last`
    while (q < q_end) {
      if (n.nocase) {  // (only for the 64 KB sample estimate: a plain loop is fine)
        while (q < q_end and lower(*q) != lower(key)) {
          ++q;
        }
      } else {
        q = static_cast<char const*>(std::memchr(q, key, static_cast<std::size_t>(q_end - q)));
        if (q == nullptr) {
          return last;
        }
      }
      if (q >= q_end) {
        return last;
      }
      char const* start = q - n.rare;
      if (same_text(start, n.text, n.nocase) and (!n.word or whole_word(start, n.text.size(), p, last))) {
        return start;
      }
      ++q;
    }
    return last;
  }

  // How many lines of `sample` contain at least one needle (estimated: occurrences, capped).
  auto hit_fraction(std::vector<Needle> const& needles, std::string_view sample) -> double
  {
    char const* b = sample.data();
    char const* e = b + sample.size();
    std::size_t lines = static_cast<std::size_t>(std::count(b, e, '\n')) + 1, hits = 0;
    for (auto const& n : needles) {
      for (char const* p = find(n, b, e); p < e; p = find(n, p + 1, e)) {
        ++hits;
      }
    }
    return std::min(1.0, static_cast<double>(hits) / static_cast<double>(lines));
  }

  // Finds every occurrence of several strings in one pass over the buffer, three ways:
  //  - one string: memchr for its rare byte (the C library already vectorizes memchr), or, when
  //    that byte isn't rare enough, its two rarest bytes at once (simd find_pair);
  //  - a few strings whose rare bytes really are rare: any of those bytes (simd find_any), then
  //    the strings that use the byte found;
  //  - otherwise, where the CPU has a byte shuffle: Teddy (simd teddy).  Every string gets a
  //    3-byte fingerprint (its rarest 3-byte window; shorter strings get "any byte" in the
  //    missing places) and the strings are split into 8 groups.  Each fingerprint byte is split
  //    into its low and high 4 bits; for each of the 3 positions two 16-entry tables say which
  //    groups (one bit each) have that half-byte there.  A shuffle does a vector's worth of
  //    lookups at once, so many text positions are tested against every string in six lookups,
  //    however many strings there are.  Where a group survives, only its strings are compared.
  // The vector code is in src/simd, one file per instruction set, chosen at run time.
  // A candidate position is where a string's fingerprint (rare byte, or 3-byte window) starts;
  // verify() works back from it to where the string would start.
  class MultiScanner
  {
  public:
    MultiScanner(std::vector<Needle> const& needles, std::string_view sample)
      : m_needles{ needles }, m_simd{ alf::simd::active() }
    {
      m_teddy = needles.size() >= 2 and m_simd.fast_teddy and rare_byte_stop_interval(needles, sample) < TEDDY_BELOW_INTERVAL;
      if (m_teddy) {
        build_teddy(sample);
        return;
      }
      if (needles.size() == 1 and !needles[0].nocase and needles[0].text.size() >= 2 and
          rare_byte_stop_interval(needles, sample) < PAIR_BELOW_INTERVAL) {
        build_pair(sample);
        return;
      }
      m_set.count = 0;
      for (bool& b : m_set.member) {
        b = false;
      }
      for (std::size_t i = 0; i < needles.size(); ++i) {
        auto const b = static_cast<unsigned char>(needles[i].text[needles[i].rare]);
        unsigned char const keys[2] = { needles[i].nocase ? lower(b) : b, needles[i].nocase ? upper(b) : b };
        for (unsigned char key : keys) {
          if (!m_set.member[key]) {
            m_set.member[key] = true;
            m_set.bytes[m_set.count++] = key;
            m_by_key[key].push_back(static_cast<std::uint16_t>(i));
          }
          else if (m_by_key[key].empty() or m_by_key[key].back() != i) {
            m_by_key[key].push_back(static_cast<std::uint16_t>(i));
          }
        }
      }
    }

    // The next candidate at or after `p` (before `last`), or `last`.  Bytes up to `limit`
    // (>= last, inside the same buffer) may be read but never reported: that lets a range
    // shorter than a vector, like the rest of one line, still be checked a vector at a time.
    [[nodiscard]] auto next_candidate(char const* p, char const* last, char const* limit) const -> char const*
    {
      if (m_teddy) {
        return m_simd.teddy(m_tables, p, last, limit);
      }
      if (m_use_pair) {
        return m_simd.find_pair(m_pair, p, last, limit);
      }
      if (m_set.count == 1) {
        auto q = static_cast<char const*>(std::memchr(p, m_set.bytes[0], static_cast<std::size_t>(last - p)));
        return q ? q : last;
      }
      return m_simd.find_any(m_set, p, last, limit);
    }

    /** Teddy's measured cost on the sample: stops plus strings compared, per byte; 0 if not Teddy. */
    [[nodiscard]] auto teddy_cost() const -> double { return m_teddy ? m_teddy_cost : 0; }

    // Start of the line containing `p` (not before `floor`, itself a line start).
    [[nodiscard]] auto line_start(char const* p, char const* floor) const -> char const*
    {
      return m_simd.line_start(p, floor);
    }

    // Call on_hit(index) for every string whose fingerprint is at `q` and that occurs there
    // entirely within [first, last).
    template <typename F>
    void verify(char const* q, char const* first, char const* last, F&& on_hit) const
    {
      auto const check = [&](std::uint16_t i, std::size_t offset) {
        Needle const& n = m_needles[i];
        char const* start = q - offset;
        if (start >= first and static_cast<std::size_t>(last - start) >= n.text.size() and
            same_text(start, n.text, n.nocase) and (!n.word or whole_word(start, n.text.size(), first, last))) {
          on_hit(i);
        }
      };
      if (m_teddy) {
        for (unsigned g = groups_at(q, last); g != 0; g &= g - 1) {
          for (std::uint16_t i : m_groups[static_cast<std::size_t>(std::countr_zero(g))]) {
            Needle const& n = m_needles[i];
            if (n.nocase ? lower(*q) == lower(n.text[m_window[i]]) : *q == n.text[m_window[i]]) {  // a group passes many near misses
              check(i, m_window[i]);
            }
          }
        }
        return;
      }
      if (m_use_pair) {
        check(0, m_pair_first);
        return;
      }
      for (std::uint16_t i : m_by_key[static_cast<unsigned char>(*q)]) {
        check(i, m_needles[i].rare);
      }
    }

  private:
    // The rare-byte scanner skips a vector or more per step while its bytes really are rare,
    // and then beats Teddy's fixed cost per byte; when they turn up often it stops constantly
    // and Teddy wins.  Measured on the benchmark log: rare bytes every 219 bytes (ERROR, WARN)
    // favoured the rare-byte scanner (33 vs 37 ms), every 105 bytes or less favoured Teddy (49
    // vs 67 ms for two strings, 113 vs 213 for sixteen).
    static constexpr double TEDDY_BELOW_INTERVAL = 128;

    // One string: memchr on a single byte runs at about twice the speed of a two-byte scan while
    // it rarely stops, but each stop costs about 10 ns; the two-byte scan costs the same whatever
    // the bytes.  Measured on 1.6 GB of subtitles (M4 Max, one core): a byte every 6,100 bytes
    // took 30 ms with memchr and 59 with the pair; a byte every 300 bytes, 83 and 56.  They
    // break even at a stop every 500-600 bytes.
    static constexpr double PAIR_BELOW_INTERVAL = 512;

    // The string's two rarest bytes in the sample (different byte values where it has them).
    void build_pair(std::string_view sample)
    {
      std::size_t freq[256] = {};
      for (unsigned char c : sample) {
        ++freq[c];
      }
      std::string_view const t = m_needles[0].text;
      auto const f = [&](std::size_t i) { return freq[static_cast<unsigned char>(t[i])]; };
      std::size_t const r1 = m_needles[0].rare;
      std::size_t r2 = r1 == 0 ? 1 : 0;
      for (std::size_t i = 0; i < t.size(); ++i) {
        if (i == r1) {
          continue;
        }
        bool const differs = t[i] != t[r1], best_differs = t[r2] != t[r1];
        if ((differs and !best_differs) or (differs == best_differs and f(i) < f(r2))) {
          r2 = i;
        }
      }
      m_use_pair = true;
      m_pair_first = std::min(r1, r2);
      std::size_t const second = std::max(r1, r2);
      m_pair = { static_cast<unsigned char>(t[m_pair_first]), static_cast<unsigned char>(t[second]),
                 static_cast<unsigned>(second - m_pair_first) };
    }

    // Average bytes between stops of the rare-byte scanner, estimated from the sample.
    static auto rare_byte_stop_interval(std::vector<Needle> const& needles, std::string_view sample) -> double
    {
      std::array<bool, 256> key{};
      for (auto const& n : needles) {
        key[static_cast<unsigned char>(n.text[n.rare])] = true;
      }
      std::size_t stops = 0;
      for (unsigned char c : sample) {
        stops += key[c];
      }
      return static_cast<double>(sample.size() + 1) / static_cast<double>(stops + 1);
    }

    // Which Teddy groups could have a fingerprint starting at q, from the bytes before `end`
    // (bytes past it count as "any").
    [[nodiscard]] auto groups_at(char const* q, char const* end) const -> unsigned
    {
      unsigned m = 0xFF;
      for (int k = 0; k < 3 and q + k < end; ++k) {
        auto const b = static_cast<unsigned char>(q[k]);
        m &= static_cast<unsigned>(m_tables.lo[k][b & 15u] & m_tables.hi[k][b >> 4]);
      }
      return m;
    }

    void build_teddy(std::string_view sample)
    {
      double freq[256];
      for (double& f : freq) {
        f = 1;
      }
      for (unsigned char c : sample) {
        freq[c] += 1;
      }
      // each string's rarest 3-byte window (estimated as the product of its bytes' frequencies)
      m_window.resize(m_needles.size());
      std::vector<std::string_view> fp(m_needles.size());
      for (std::size_t i = 0; i < m_needles.size(); ++i) {
        std::string_view const t = m_needles[i].text;
        std::size_t best = 0;
        double best_cost = -1;
        auto const fq = [&](char ch) {
          auto const b = static_cast<unsigned char>(ch);
          return m_needles[i].nocase and lower(b) != upper(b) ? freq[lower(b)] + freq[upper(b)] - 1 : freq[b];
        };
        for (std::size_t o = 0; o + 3 <= t.size(); ++o) {
          double const c = fq(t[o]) * fq(t[o + 1]) * fq(t[o + 2]);
          if (best_cost < 0 or c < best_cost) {
            best_cost = c;
            best = o;
          }
        }
        m_window[i] = best;
        fp[i] = t.substr(best, 3);
      }
      // Strings with the same fingerprint always share a group.  How the distinct fingerprints
      // are spread over the 8 groups decides the cost: every position where a group's bit
      // survives is a stop, and every string in a surviving group is compared.  A fingerprint
      // that turns up all the time is best alone in its group, so its stops compare one string;
      // rare ones can share.  So a few layouts are tried on the sample and the cheapest kept:
      // alphabetical (similar fingerprints share a group, so its combined tables let fewer bytes
      // through), and the k most frequent fingerprints alone with the rest alphabetical.
      std::vector<std::string_view> distinct(fp);
      std::sort(distinct.begin(), distinct.end());
      distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
      std::vector<std::size_t> seen(distinct.size(), 0);  // occurrences in the sample
      std::array<std::vector<std::size_t>, 256> starting_with;
      for (std::size_t d = 0; d < distinct.size(); ++d) {
        starting_with[static_cast<unsigned char>(distinct[d][0])].push_back(d);
      }
      for (std::size_t o = 0; o < sample.size(); ++o) {
        for (std::size_t d : starting_with[static_cast<unsigned char>(sample[o])]) {
          seen[d] += sample.compare(o, distinct[d].size(), distinct[d]) == 0;
        }
      }
      std::vector<std::size_t> by_seen(distinct.size());
      for (std::size_t d = 0; d < by_seen.size(); ++d) {
        by_seen[d] = d;
      }
      std::stable_sort(by_seen.begin(), by_seen.end(), [&](std::size_t a, std::size_t b) { return seen[a] > seen[b]; });
      std::vector<std::size_t> strings_with(distinct.size(), 0);
      std::vector<std::size_t> fp_of(m_needles.size());
      for (std::size_t i = 0; i < m_needles.size(); ++i) {
        fp_of[i] = static_cast<std::size_t>(std::lower_bound(distinct.begin(), distinct.end(), fp[i]) - distinct.begin());
        ++strings_with[fp_of[i]];
      }

      std::vector<std::size_t> best_layout;  // group of each distinct fingerprint
      double best_cost = -1;
      std::size_t const singles_max = std::min<std::size_t>(GROUPS - 1, distinct.size());
      for (std::size_t singles = 0; singles <= singles_max; ++singles) {
        std::vector<std::size_t> layout(distinct.size());
        std::vector<bool> alone(distinct.size(), false);
        for (std::size_t r = 0; r < singles; ++r) {
          alone[by_seen[r]] = true;
          layout[by_seen[r]] = r;
        }
        std::size_t const rest = distinct.size() - singles, shared = GROUPS - singles;
        for (std::size_t d = 0, r = 0; d < distinct.size(); ++d) {
          if (!alone[d]) {
            layout[d] = singles + r++ * shared / std::max<std::size_t>(rest, 1);
          }
        }
        fill_tables(layout, fp, fp_of);
        std::array<double, GROUPS> size{};
        for (std::size_t d = 0; d < distinct.size(); ++d) {
          size[layout[d]] += static_cast<double>(strings_with[d]);
        }
        double cost = 0;
        char const* b = sample.data();
        char const* e = b + sample.size();
        for (char const* q = m_simd.teddy(m_tables, b, e, e); q < e; q = m_simd.teddy(m_tables, q + 1, e, e)) {
          cost += 1;  // the stop
          for (unsigned g = groups_at(q, e); g != 0; g &= g - 1) {
            cost += size[static_cast<std::size_t>(std::countr_zero(g))];
          }
        }
        if (best_cost < 0 or cost < best_cost) {
          best_cost = cost;
          best_layout = std::move(layout);
        }
      }
      m_teddy_cost = sample.empty() ? 0 : best_cost / static_cast<double>(sample.size());
      fill_tables(best_layout, fp, fp_of);
      for (std::size_t i = 0; i < m_needles.size(); ++i) {
        m_groups[best_layout[fp_of[i]]].push_back(static_cast<std::uint16_t>(i));
      }
    }

    // Teddy tables for fingerprints fp (string i's is fp[i], distinct fingerprint fp_of[i]),
    // with distinct fingerprint d in group layout[d].
    void fill_tables(std::vector<std::size_t> const& layout, std::vector<std::string_view> const& fp,
                     std::vector<std::size_t> const& fp_of)
    {
      m_tables = {};
      for (std::size_t i = 0; i < fp.size(); ++i) {
        auto const bit = static_cast<unsigned char>(1u << layout[fp_of[i]]);
        for (std::size_t k = 0; k < 3; ++k) {
          if (k < fp[i].size()) {
            auto const b = static_cast<unsigned char>(fp[i][k]);
            for (unsigned char v : { b, m_needles[i].nocase ? lower(b) : b, m_needles[i].nocase ? upper(b) : b }) {
              m_tables.lo[k][v & 15u] |= bit;
              m_tables.hi[k][v >> 4] |= bit;
            }
          } else {  // a string shorter than 3 bytes: any byte matches here
            for (int n = 0; n < 16; ++n) {
              m_tables.lo[k][n] |= bit;
              m_tables.hi[k][n] |= bit;
            }
          }
        }
      }
    }

    static constexpr std::size_t GROUPS = 8;

    std::vector<Needle> const& m_needles;
    alf::simd::Backend const& m_simd;
    // rare-byte mode
    alf::simd::ByteSet m_set{};
    std::array<std::vector<std::uint16_t>, 256> m_by_key;
    // pair mode (one string)
    bool m_use_pair{ false };
    alf::simd::BytePair m_pair{};
    std::size_t m_pair_first{ 0 };  // offset of the pair's first byte within the string
    // Teddy mode
    bool m_teddy{ false };
    double m_teddy_cost{ 0 };
    alf::simd::TeddyTables m_tables{};
    std::array<std::vector<std::uint16_t>, GROUPS> m_groups;
    std::vector<std::size_t> m_window;  // offset of each string's fingerprint within it
  };

  // The kept text, as views into the input: touching pieces merge, so a run of kept lines is
  // one view.  With a `sink` (one thread), finished pieces are handed on in batches as the list
  // grows, so memory stays bounded; without one (a worker thread) they are kept until the caller
  // takes them.
  //
  // Handing the kernel many short pieces costs far more than their bytes (measured: 100,000
  // pieces of 71 bytes take 5.1 ms to write, the same 7 MB in one piece 0.9 ms), so a batch of
  // small scattered pieces is packed into one reused buffer first.  Long runs stay views.
  class Spans
  {
  public:
    explicit Spans(alf::Output const* sink = nullptr) : m_sink{ sink } { }

    void add(char const* b, char const* e)
    {
      if (b == e) {
        return;
      }
      if (!m_list.empty() and m_list.back().data() + m_list.back().size() == b) {
        m_list.back() = std::string_view(m_list.back().data(), m_list.back().size() + static_cast<std::size_t>(e - b));
        return;
      }
      if (m_sink and m_list.size() >= FLUSH_EVERY) {
        emit();
      }
      m_list.emplace_back(b, static_cast<std::size_t>(e - b));
    }

    // Hand every piece still held to the sink.
    void finish()
    {
      if (m_sink) {
        emit();
      }
    }

    [[nodiscard]] auto list() const -> std::vector<std::string_view> const& { return m_list; }

  private:
    void emit()
    {
      std::size_t bytes = 0;
      for (auto v : m_list) {
        bytes += v.size();
      }
      if (m_list.size() > 1 and bytes / m_list.size() < PACK_BELOW) {
        m_pack.clear();
        for (auto v : m_list) {
          m_pack.append(v);
        }
        m_sink->write(m_pack);
        if (m_sink->flush) {
          m_sink->flush();  // m_pack is reused for the next batch: it must be consumed first
        }
      } else {
        for (auto v : m_list) {
          m_sink->write(v);
        }
      }
      m_list.clear();
    }

    static constexpr std::size_t FLUSH_EVERY = 4096;
    alf::Output const* m_sink;
    std::vector<std::string_view> m_list;
    std::string m_pack;
  };

  // Filter [first, last) by searching for the strings in one pass: only lines with a hit are
  // evaluated; the lines in between all get the answer `keep_others` and are skipped or kept in
  // one piece.  With `use_found`, the scanner is looking for all of the expression's strings, so
  // it records which ones a hit line contains and the line is evaluated from that, without
  // searching it again.
  // Which strings a line contains, one bit each.  Up to 64 strings that's one word, kept in a
  // register; more need an array.
  struct OneWord
  {
    std::uint64_t bits{ 0 };
    explicit OneWord(std::size_t) { }
    void clear() { bits = 0; }
    auto set(std::size_t i) -> bool  // true if the bit was new
    {
      std::uint64_t const b = std::uint64_t{ 1 } << i;
      bool const was = (bits & b) != 0;
      bits |= b;
      return !was;
    }
    [[nodiscard]] auto words() const -> std::span<std::uint64_t const> { return { &bits, 1 }; }
  };

  struct ManyWords
  {
    std::vector<std::uint64_t> bits;
    explicit ManyWords(std::size_t words) : bits(words, 0) { }
    void clear() { std::fill(bits.begin(), bits.end(), 0); }
    auto set(std::size_t i) -> bool
    {
      std::uint64_t const b = std::uint64_t{ 1 } << (i % 64);
      bool const was = (bits[i / 64] & b) != 0;
      bits[i / 64] |= b;
      return !was;
    }
    [[nodiscard]] auto words() const -> std::span<std::uint64_t const> { return bits; }
  };

  // `words`: enough for every string the scanner reports (one bit per needle).
  template <typename Found>
  void filter_range_search(char const* first, char const* last, alf::Program const& program,
                           MultiScanner const& scan, bool keep_others, bool use_found, std::size_t words, Spans& out)
  {
    Found found{ words };
    char const* cursor = first;  // start of the first line not yet handled
    char const* p = first;       // where the scan continues
    for (;;) {
      char const* q = scan.next_candidate(p, last, last);
      if (q == last) {
        if (keep_others) {
          out.add(cursor, last);
        }
        return;
      }
      found.clear();
      bool hit = false;
      scan.verify(q, cursor, last, [&](std::size_t i) {
        hit = true;
        found.set(i);
      });
      if (!hit) {
        p = q + 1;
        continue;
      }
      char const* ls = scan.line_start(q, cursor);
      char const* e = line_end(q, last);
      char const* text_end = (e > ls and e[-1] == '\r') ? e - 1 : e;
      char const* next = e < last ? e + 1 : e;
      std::string_view const line(ls, static_cast<std::size_t>(text_end - ls));
      bool keep;
      if (use_found) {
        // Scan the rest of the line only while its answer could still change: in an exclude
        // list the first unwanted string found already settles it.
        std::optional<bool> answer = program.settled(line, found.words());
        for (char const* c = q + 1; !answer and (c = scan.next_candidate(c, e, last)) < e; ++c) {
          bool changed = false;
          scan.verify(c, ls, e, [&](std::size_t i) { changed |= found.set(i); });
          if (changed) {
            answer = program.settled(line, found.words());
          }
        }
        keep = answer ? *answer : program.matches_found(line, found.words());
      } else {
        keep = program.matches(line);
      }
      if (keep_others) {
        out.add(cursor, ls);
      }
      if (keep) {
        out.add(ls, next);
      }
      cursor = next;
      p = next;
    }
  }

  // Teddy's cost on the sample (stops plus strings compared, per byte) above which the DFA is
  // faster.  The DFA (four lanes) costs about the same per byte for any list: one core, 142 MB
  // benchmark log, 100-110 ms from 24 to 1,024 strings.  Teddy grows with its measured cost: 0.23
  // at 40 strings took 97 ms, 0.36 at 48 took 110.  They cross at about 0.27.
  constexpr double AHO_ABOVE_COST = 0.27;

  // filter_range_search with an Aho-Corasick DFA instead of the scanner: every byte is one table
  // step, whatever the number of strings.  None of the strings contains '\n', so every match lies
  // within one line and the DFA can restart at each line.  Lines without a match get
  // `keep_others`; a line with one is decided as in filter_range_search (from the strings found
  // when `use_found`, reading on only until the answer is settled).
  //
  // A step can't start before the previous one's table load has finished, so one DFA runs at the
  // latency of a load: about 1.1 ns a byte on an M4 Max, however few instructions the loop has.
  // So the range is cut at line breaks into LANES pieces, and one loop steps a DFA through each
  // of them: the loads are independent and overlap (measured on a bare loop over 142 MB: one
  // chain 159 ms, two 80, four 42, eight 35).  Lane 0 writes to `out` as it goes; the others
  // collect their pieces and are appended in order at the end.
  // `Check`: some string needs its matches checked (see `real` below); when none does, the
  // check is compiled out, so plain strings cost exactly what they did before i'...' and w'...'.
  template <typename Found, bool Check>
  void filter_range_aho(char const* first, char const* last, alf::Program const& program,
                        alf::detail::AhoCorasick const& ac, std::vector<Needle> const& needles, bool keep_others,
                        bool use_found, std::size_t words, std::size_t min_lane_bytes, Spans& out)
  {
    // A DFA match is a real one unless the DFA folded case for a case-sensitive string, or the
    // string is w'...' and the match is part of a longer word.
    bool fold = false;
    for (Needle const& n : needles) {
      fold |= n.nocase;
    }
    // the string `i` ending at `at`, within the line [ls, e)
    auto const real = [&](std::uint32_t i, char const* at, char const* ls, char const* e) {
      if constexpr (!Check) {
        return true;
      }
      Needle const& n = needles[i];
      char const* start = at + 1 - n.text.size();
      return (!fold or n.nocase or std::memcmp(start, n.text.data(), n.text.size()) == 0) and
             (!n.word or whole_word(start, n.text.size(), ls, e));
    };
    constexpr unsigned LANES = 4;
    alf::simd::Backend const& simd = alf::simd::active();
    unsigned const lanes = static_cast<std::size_t>(last - first) >= LANES * min_lane_bytes ? LANES : 1;

    char const* p[LANES];       // next byte to step
    char const* end[LANES];     // end of the lane
    char const* cursor[LANES];  // start of the lane's first line not yet handled
    std::uint32_t state[LANES];
    std::array<Spans, LANES - 1> later;  // lanes 1.. collect here
    Spans* sink[LANES];
    std::vector<Found> found;
    char const* cut = first;
    for (unsigned k = 0; k < lanes; ++k) {
      char const* e = k + 1 == lanes ? last : first + static_cast<std::size_t>(last - first) * (k + 1) / lanes;
      if (e < last) {
        e = std::max(e, cut);
        e = line_end(e, last);
        e = e < last ? e + 1 : last;
      }
      p[k] = cursor[k] = cut;
      end[k] = e;
      state[k] = alf::detail::AhoCorasick::start;
      sink[k] = k == 0 ? &out : &later[k - 1];
      found.emplace_back(words);
      cut = e;
    }

    // the line containing a match that ends at p[k]: decide it and move lane k past it; or, if
    // none of the strings ending there is a real match (see `real`), return false and leave the
    // lane as it is, to step on from the next byte
    auto const handle = [&](unsigned k) -> bool {
      char const* ls = simd.line_start(p[k], cursor[k]);
      char const* e = line_end(p[k], end[k]);
      if constexpr (Check) {
        bool any = false;
        for (std::uint32_t i : ac.ending(state[k])) {
          any = any or real(i, p[k], ls, e);
        }
        if (!any) {
          return false;
        }
      }
      char const* text_end = (e > ls and e[-1] == '\r') ? e - 1 : e;
      char const* next = e < end[k] ? e + 1 : e;
      std::string_view const line(ls, static_cast<std::size_t>(text_end - ls));
      bool keep;
      if (use_found) {
        Found& f = found[k];
        f.clear();
        for (std::uint32_t i : ac.ending(state[k])) {
          if (real(i, p[k], ls, e)) {
            f.set(i);
          }
        }
        std::uint32_t st = state[k];
        std::optional<bool> answer = program.settled(line, f.words());
        for (char const* c = p[k] + 1; !answer and c < e; ++c) {
          st = ac.step(st, static_cast<unsigned char>(*c));
          if (ac.matches(st)) {
            bool changed = false;
            for (std::uint32_t i : ac.ending(st)) {
              if (real(i, c, ls, e)) {
                changed |= f.set(i);
              }
            }
            if (changed) {
              answer = program.settled(line, f.words());
            }
          }
        }
        keep = answer ? *answer : program.matches_found(line, f.words());
      } else {
        keep = program.matches(line);
      }
      if (keep_others) {
        sink[k]->add(cursor[k], ls);
      }
      if (keep) {
        sink[k]->add(ls, next);
      }
      cursor[k] = p[k] = next;
      state[k] = alf::detail::AhoCorasick::start;
      return true;
    };

    // all lanes together while each has bytes left.  The states and positions are plain locals
    // here (copied back only when a lane matches): kept in the arrays, which `handle` sees, a
    // compiler may store and reload them on every byte, and that round trip lands right on the
    // chain of dependent loads the lanes are there to overlap.
    if (lanes == LANES) {
      static_assert(LANES == 4);
      for (;;) {
        std::size_t steps = static_cast<std::size_t>(end[0] - p[0]);
        for (unsigned k = 1; k < LANES; ++k) {
          steps = std::min(steps, static_cast<std::size_t>(end[k] - p[k]));
        }
        if (steps == 0) {
          break;
        }
        char const *q0 = p[0], *q1 = p[1], *q2 = p[2], *q3 = p[3];
        std::uint32_t s0 = state[0], s1 = state[1], s2 = state[2], s3 = state[3];
        char const* const stop = q0 + steps;
        bool hit = false;
        while (q0 < stop) {
          s0 = ac.step(s0, static_cast<unsigned char>(*q0));
          s1 = ac.step(s1, static_cast<unsigned char>(*q1));
          s2 = ac.step(s2, static_cast<unsigned char>(*q2));
          s3 = ac.step(s3, static_cast<unsigned char>(*q3));
          // match states are numbered last, so some lane matched exactly when the largest state did
          if (ac.matches(std::max(std::max(s0, s1), std::max(s2, s3)))) {
            hit = true;
            break;
          }
          ++q0, ++q1, ++q2, ++q3;
        }
        p[0] = q0, p[1] = q1, p[2] = q2, p[3] = q3;
        state[0] = s0, state[1] = s1, state[2] = s2, state[3] = s3;
        if (!hit) {
          continue;  // `steps` used up: some lane is at its end
        }
        for (unsigned k = 0; k < LANES; ++k) {  // the byte at p[k] has been stepped in every lane
          if (!ac.matches(state[k]) or !handle(k)) {
            ++p[k];
          }
        }
      }
    }
    // then each lane's rest on its own
    for (unsigned k = 0; k < lanes; ++k) {
      while (p[k] < end[k]) {
        state[k] = ac.step(state[k], static_cast<unsigned char>(*p[k]));
        if (!ac.matches(state[k]) or !handle(k)) {
          ++p[k];
        }
      }
      if (keep_others) {
        sink[k]->add(cursor[k], end[k]);
      }
    }
    for (unsigned k = 1; k < lanes; ++k) {
      for (std::string_view v : later[k - 1].list()) {
        out.add(v.data(), v.data() + v.size());
      }
    }
  }

  // Filter [first, last) line by line, adding kept lines to `out`.
  void filter_range(char const* first, char const* last, alf::Program const& program, Spans& out)
  {
    for (char const* p = first; p < last;) {
      char const* e = line_end(p, last);
      char const* text_end = (e > p and e[-1] == '\r') ? e - 1 : e;
      char const* next = e < last ? e + 1 : e;
      if (program.matches(std::string_view(p, static_cast<std::size_t>(text_end - p)))) {
        out.add(p, next);
      }
      p = next;
    }
  }
}


alf::Program::Program(std::vector<types::TokenBase> const& postfix)
{
  std::size_t depth = 0;
  for (auto const& t : postfix) {
    switch (t.type) {
      case types::TYPE_TOKEN::SUBSTR:
      case types::TYPE_TOKEN::REGEX:
        ++depth;
        break;
      case types::TYPE_TOKEN::OPERATOR_NOT:
        if (depth < 1) {
          throw std::runtime_error("malformed expression: nothing to negate");
        }
        break;
      case types::TYPE_TOKEN::OPERATOR_AND:
      case types::TYPE_TOKEN::OPERATOR_OR:
      case types::TYPE_TOKEN::OPERATOR_XOR:
        if (depth < 2) {
          throw std::runtime_error("malformed expression: an operator is missing an operand");
        }
        --depth;
        break;
      default:
        throw std::runtime_error("malformed expression: unexpected " + types::type_token_to_string(t.type));
    }
    m_depth = std::max(m_depth, depth);
    m_ops.push_back({ t.type, t.required, t.value, t.regex });
    m_ops.back().nocase = t.nocase;
    m_ops.back().word = t.word;
  }
  if (!postfix.empty() and depth != 1) {
    throw std::runtime_error("malformed expression: operands without an operator between them");
  }

  std::vector<std::size_t> operands;
  for (std::size_t i = 0; i < m_ops.size(); ++i) {
    auto const type = m_ops[i].type;
    Node n{ type, i, 0, 0 };
    if (type == types::TYPE_TOKEN::OPERATOR_NOT) {
      n.left = operands.back();
      operands.pop_back();
    } else if (types::is_operator(type)) {
      n.right = operands.back();
      operands.pop_back();
      n.left = operands.back();
      operands.pop_back();
    }
    m_nodes.push_back(n);
    operands.push_back(m_nodes.size() - 1);
  }

  bool searchable = !m_ops.empty();
  for (auto const& op : m_ops) {
    if (op.type == types::TYPE_TOKEN::REGEX or (op.type == types::TYPE_TOKEN::SUBSTR and op.literal.empty())) {
      searchable = false;
    }
    if (op.type == types::TYPE_TOKEN::SUBSTR) {
      Literal const lit{ op.literal, op.nocase, op.word };
      if (std::find(m_literals.begin(), m_literals.end(), lit) == m_literals.end()) {
        m_literals.push_back(lit);
      }
    }
  }
  for (auto& op : m_ops) {
    if (op.type == types::TYPE_TOKEN::SUBSTR) {
      Literal const lit{ op.literal, op.nocase, op.word };
      op.literal_index = static_cast<std::size_t>(std::find(m_literals.begin(), m_literals.end(), lit) - m_literals.begin());
    }
  }
  if (searchable) {
    // evaluate once as if no string were found: a wanted string is false, an unwanted one true
    std::vector<char> stk;
    for (auto const& op : m_ops) {
      switch (op.type) {
        case types::TYPE_TOKEN::SUBSTR: stk.push_back(!op.required); break;
        case types::TYPE_TOKEN::OPERATOR_NOT: stk.back() = !stk.back(); break;
        case types::TYPE_TOKEN::OPERATOR_AND: { char b = stk.back(); stk.pop_back(); stk.back() = stk.back() and b; break; }
        case types::TYPE_TOKEN::OPERATOR_OR: { char b = stk.back(); stk.pop_back(); stk.back() = stk.back() or b; break; }
        case types::TYPE_TOKEN::OPERATOR_XOR: { char b = stk.back(); stk.pop_back(); stk.back() = stk.back() != b; break; }
        default: break;
      }
    }
    m_without_literals = stk.back() != 0;
    // Three-valued evaluation only gains certainty as more strings are found, so a string whose
    // presence alone settles the answer settles it in every line that contains it.
    m_drops_alone.assign(found_words(), 0);
    m_keeps_alone.assign(found_words(), 0);
    std::vector<std::uint64_t> only(found_words(), 0);
    for (std::size_t i = 0; i < m_literals.size(); ++i) {
      std::uint64_t const bit = std::uint64_t{ 1 } << (i % 64);
      only[i / 64] = bit;
      if (auto const r = eval_settled(m_nodes.size() - 1, {}, only.data())) {
        (*r ? m_keeps_alone : m_drops_alone)[i / 64] |= bit;
      }
      only[i / 64] = 0;
    }
  }
}


auto alf::Program::matches(std::string_view line) const -> bool
{
  return m_nodes.empty() or eval(m_nodes.size() - 1, line);
}


auto alf::Program::eval(std::size_t node, std::string_view line) const -> bool
{
  Node const& n = m_nodes[node];
  switch (n.type) {
    case types::TYPE_TOKEN::SUBSTR: {
      Op const& op = m_ops[n.op];
      return contains(line, op.literal, op.nocase, op.word) == op.required;
    }
    case types::TYPE_TOKEN::REGEX: {
      Op const& op = m_ops[n.op];
      return std::regex_search(line.begin(), line.end(), *op.regex) == op.required;
    }
    case types::TYPE_TOKEN::OPERATOR_NOT:
      return !eval(n.left, line);
    case types::TYPE_TOKEN::OPERATOR_AND:
      return eval(n.left, line) and eval(n.right, line);
    case types::TYPE_TOKEN::OPERATOR_OR:
      return eval(n.left, line) or eval(n.right, line);
    case types::TYPE_TOKEN::OPERATOR_XOR:
      return eval(n.left, line) != eval(n.right, line);
    default:
      return false;
  }
}


auto alf::Program::matches_found(std::string_view line, std::span<std::uint64_t const> found) const -> bool
{
  return m_nodes.empty() or eval_found(m_nodes.size() - 1, line, found.data());
}


auto alf::Program::matches_found(std::string_view line, std::uint64_t found) const -> bool
{
  return matches_found(line, std::span<std::uint64_t const>(&found, 1));
}


namespace
{
  inline auto has(std::uint64_t const* found, std::size_t i) -> bool { return ((found[i / 64] >> (i % 64)) & 1u) != 0; }
}


auto alf::Program::eval_found(std::size_t node, std::string_view line, std::uint64_t const* found) const -> bool
{
  Node const& n = m_nodes[node];
  switch (n.type) {
    case types::TYPE_TOKEN::SUBSTR: {
      Op const& op = m_ops[n.op];
      return has(found, op.literal_index) == op.required;
    }
    case types::TYPE_TOKEN::REGEX: {
      Op const& op = m_ops[n.op];
      return std::regex_search(line.begin(), line.end(), *op.regex) == op.required;
    }
    case types::TYPE_TOKEN::OPERATOR_NOT:
      return !eval_found(n.left, line, found);
    case types::TYPE_TOKEN::OPERATOR_AND:
      return eval_found(n.left, line, found) and eval_found(n.right, line, found);
    case types::TYPE_TOKEN::OPERATOR_OR:
      return eval_found(n.left, line, found) or eval_found(n.right, line, found);
    case types::TYPE_TOKEN::OPERATOR_XOR:
      return eval_found(n.left, line, found) != eval_found(n.right, line, found);
    default:
      return false;
  }
}


auto alf::Program::settled(std::string_view line, std::span<std::uint64_t const> found) const -> std::optional<bool>
{
  if (m_nodes.empty()) {
    return true;
  }
  for (std::size_t w = 0; w < m_drops_alone.size(); ++w) {
    if ((found[w] & m_drops_alone[w]) != 0) {
      return false;
    }
    if ((found[w] & m_keeps_alone[w]) != 0) {
      return true;
    }
  }
  return eval_settled(m_nodes.size() - 1, line, found.data());
}


auto alf::Program::settled(std::string_view line, std::uint64_t found) const -> std::optional<bool>
{
  return settled(line, std::span<std::uint64_t const>(&found, 1));
}


// Three-valued (true / false / not yet known) evaluation: a string already found is known to be
// present; one not found yet might still turn up later in the line, so it is unknown.
auto alf::Program::eval_settled(std::size_t node, std::string_view line, std::uint64_t const* found) const
  -> std::optional<bool>
{
  Node const& n = m_nodes[node];
  switch (n.type) {
    case types::TYPE_TOKEN::SUBSTR: {
      Op const& op = m_ops[n.op];
      if (has(found, op.literal_index)) {
        return op.required;
      }
      return std::nullopt;
    }
    case types::TYPE_TOKEN::REGEX: {
      Op const& op = m_ops[n.op];
      return std::regex_search(line.begin(), line.end(), *op.regex) == op.required;
    }
    case types::TYPE_TOKEN::OPERATOR_NOT: {
      auto a = eval_settled(n.left, line, found);
      return a ? std::optional<bool>{ !*a } : std::nullopt;
    }
    case types::TYPE_TOKEN::OPERATOR_AND: {
      auto a = eval_settled(n.left, line, found);
      if (a == false) {
        return false;
      }
      auto b = eval_settled(n.right, line, found);
      if (b == false) {
        return false;
      }
      return (a and b) ? std::optional<bool>{ true } : std::nullopt;
    }
    case types::TYPE_TOKEN::OPERATOR_OR: {
      auto a = eval_settled(n.left, line, found);
      if (a == true) {
        return true;
      }
      auto b = eval_settled(n.right, line, found);
      if (b == true) {
        return true;
      }
      return (a and b) ? std::optional<bool>{ false } : std::nullopt;
    }
    case types::TYPE_TOKEN::OPERATOR_XOR: {
      auto a = eval_settled(n.left, line, found);
      auto b = eval_settled(n.right, line, found);
      return (a and b) ? std::optional<bool>{ *a != *b } : std::nullopt;
    }
    default:
      return std::nullopt;
  }
}


auto alf::Program::required_literals(std::function<double(std::string_view)> const& cost) const
  -> std::optional<std::vector<Literal>>
{
  using Set = std::optional<std::vector<Literal>>;
  auto const total = [&](std::vector<Literal> const& v) {
    double c = 0;
    for (auto const& s : v) {
      c += cost(s.text);
    }
    return c;
  };
  std::vector<Set> stk;
  for (auto const& op : m_ops) {
    switch (op.type) {
      case types::TYPE_TOKEN::SUBSTR:
        stk.push_back(op.required and !op.literal.empty() ? Set{ { Literal{ op.literal, op.nocase, op.word } } } : std::nullopt);
        break;
      case types::TYPE_TOKEN::REGEX: {
        std::string prefix = op.required ? regex_required_prefix(op.literal) : std::string{};
        stk.push_back(prefix.empty() ? std::nullopt : Set{ { Literal{ prefix } } });
        break;
      }
      case types::TYPE_TOKEN::OPERATOR_NOT:
        stk.back() = std::nullopt;
        break;
      case types::TYPE_TOKEN::OPERATOR_AND: {  // either side's set will do: keep the cheaper one
        Set b = std::move(stk.back());
        stk.pop_back();
        Set& a = stk.back();
        if (!a or (b and total(*b) < total(*a))) {
          a = std::move(b);
        }
        break;
      }
      case types::TYPE_TOKEN::OPERATOR_OR:
      case types::TYPE_TOKEN::OPERATOR_XOR: {  // true only if one side is: needs both sides' sets
        Set b = std::move(stk.back());
        stk.pop_back();
        Set& a = stk.back();
        if (a and b) {
          for (auto& s : *b) {
            if (std::find(a->begin(), a->end(), s) == a->end()) {
              a->push_back(std::move(s));
            }
          }
        } else {
          a = std::nullopt;
        }
        break;
      }
      default:
        break;
    }
  }
  return stk.empty() ? std::nullopt : std::move(stk.back());
}


auto alf::regex_required_prefix(std::string_view pattern) -> std::string
{
  if (pattern.find('|') != std::string_view::npos) {
    return {};  // an alternative could match without the prefix
  }
  std::size_t i = (!pattern.empty() and pattern[0] == '^') ? 1 : 0;
  std::string prefix;
  for (; i < pattern.size(); ++i) {
    char c = pattern[i];
    if (std::strchr("\\.^$*+?()[]{}", c) != nullptr) {
      break;
    }
    prefix += c;
  }
  // a quantifier that allows zero repeats makes the character before it optional
  if (i < pattern.size() and std::strchr("*?{", pattern[i]) != nullptr and !prefix.empty()) {
    prefix.pop_back();
  }
  return prefix;
}


void alf::filter(std::string_view data, Program const& program, unsigned threads, Output const& out,
                 double max_hit_fraction)
{
  auto const& write = out.write;
  char const* begin = data.data();
  char const* end = begin + data.size();

  // Search for strings when that skips most lines, preferring a set of strings every passing
  // line must contain (the other lines are simply dropped); failing that, all of the
  // expression's strings, if lines without any of them share one answer.  Read line by line
  // when most lines would be hits anyway, where searching only adds work.
  auto const sample = data.substr(0, SAMPLE_BYTES);
  auto const cost = [&](std::string_view lit) {
    double n = 1;
    for (auto p = sample.find(lit); p != std::string_view::npos; p = sample.find(lit, p + 1)) {
      ++n;
    }
    return n;
  };
  std::vector<Literal> search_for;  // owns the text the needles point into
  bool keep_others = false;
  bool all_literals = false;
  if (auto required = program.required_literals(cost)) {
    search_for = std::move(*required);
  } else if (auto without = program.answer_without_literals(); without and !program.literals().empty()) {
    search_for = program.literals();
    keep_others = *without;
    all_literals = true;
  }
  std::vector<Needle> needles = make_needles(search_for, sample);
  // Evaluating a hit line from the strings the scanner found needs every string, and none that
  // could match across a line break (or include the "\r" a line is matched without).
  bool const use_found = all_literals and
    std::none_of(search_for.begin(), search_for.end(), [](Literal const& t) {
      return t.text.find_first_of("\r\n") != std::string::npos;
    });
  // Searching for a few required strings pays when most lines don't have them.  Searching for all
  // of the strings while recording which ones each hit line has pays even when most lines are
  // hits (it replaces one search per string per line with one pass), so it is skipped only when
  // reading every line is forced (max_hit_fraction < 0).
  if (!needles.empty() and (use_found ? max_hit_fraction < 0 : hit_fraction(needles, sample) > max_hit_fraction)) {
    needles.clear();
  }
  MultiScanner const scanner{ needles, sample };
  // A long list: a DFA instead of Teddy once Teddy's measured cost on the sample says it's slower
  // (ALF_MULTI=aho / teddy forces the choice, for tests and measurement).
  std::optional<alf::detail::AhoCorasick> aho;
  std::size_t aho_lane_bytes = 64 << 10;  // smaller ranges use one lane; ALF_MULTI=aho-lanes: always four (tests)
  if (!needles.empty() and std::none_of(needles.begin(), needles.end(), [](Needle const& n) {
        return n.text.find('\n') != std::string_view::npos;
      })) {
    char const* force = std::getenv("ALF_MULTI");
    std::string_view const forced = force ? force : "";
    if (forced == "aho" or forced == "aho-lanes" or (forced != "teddy" and scanner.teddy_cost() > AHO_ABOVE_COST)) {
      std::vector<std::string_view> texts;
      bool fold = false;
      for (Needle const& n : needles) {
        texts.push_back(n.text);
        fold |= n.nocase;
      }
      aho.emplace(texts, fold);
      if (forced == "aho-lanes") {
        aho_lane_bytes = 1;
      }
    }
  }
  auto const run = [&](char const* first, char const* last, Spans& out) {
    if (needles.empty()) {
      filter_range(first, last, program, out);
    } else {
      // the needles are the program's literals in order when use_found; otherwise they are
      // required strings (possibly regex prefixes) and `found` is only a hit marker
      std::size_t const words = std::max(program.found_words(), (needles.size() + 63) / 64);
      if (aho) {
        bool const check = std::any_of(needles.begin(), needles.end(), [](Needle const& n) { return n.nocase or n.word; });
        auto const go = [&]<typename Found, bool Check>() {
          filter_range_aho<Found, Check>(first, last, program, *aho, needles, keep_others, use_found, words, aho_lane_bytes, out);
        };
        if (words <= 1) {
          check ? go.template operator()<OneWord, true>() : go.template operator()<OneWord, false>();
        } else {
          check ? go.template operator()<ManyWords, true>() : go.template operator()<ManyWords, false>();
        }
      } else if (words <= 1) {
        filter_range_search<OneWord>(first, last, program, scanner, keep_others, use_found, words, out);
      } else {
        filter_range_search<ManyWords>(first, last, program, scanner, keep_others, use_found, words, out);
      }
    }
  };
  threads = std::max(1u, std::min<unsigned>(threads, static_cast<unsigned>(data.size() / MIN_BYTES_PER_THREAD)));

  if (threads == 1) {
    Spans spans{ &out };
    run(begin, end, spans);
    spans.finish();
    return;
  }

  // cut at line breaks: piece t ends just after the first '\n' at or past t/threads of the data
  std::vector<char const*> cuts{ begin };
  for (unsigned t = 1; t < threads; ++t) {
    char const* c = begin + data.size() * t / threads;
    c = std::max(c, cuts.back());
    c = c < end ? line_end(c, end) : end;
    cuts.push_back(c < end ? c + 1 : end);
  }
  cuts.push_back(end);

  // Each thread's result: its views, or (when those views are many small scattered pieces) one
  // contiguous copy of them.  Writing a hundred thousand short views costs the kernel more than
  // it saves, and the main thread would be making that copy alone; packing them here spreads
  // the copy over every thread and leaves the main thread a few large writes.
  struct Piece
  {
    Spans spans;
    std::string packed;
  };
  std::vector<Piece> outs(threads);
  std::vector<std::exception_ptr> errors(threads);
  std::vector<std::thread> pool;
  pool.reserve(threads);
  for (unsigned t = 0; t < threads; ++t) {
    pool.emplace_back([&, t] {
      try {
        run(cuts[t], cuts[t + 1], outs[t].spans);
        auto const& views = outs[t].spans.list();
        std::size_t bytes = 0;
        for (auto v : views) {
          bytes += v.size();
        }
        if (!views.empty() and bytes / views.size() < PACK_BELOW) {
          outs[t].packed.reserve(bytes);
          for (auto v : views) {
            outs[t].packed.append(v);
          }
        }
      } catch (...) {
        errors[t] = std::current_exception();
      }
    });
  }
  for (auto& th : pool) {
    th.join();
  }
  for (auto const& e : errors) {
    if (e) {
      std::rethrow_exception(e);
    }
  }
  for (auto const& o : outs) {
    if (!o.packed.empty()) {
      write(o.packed);
    } else {
      for (auto v : o.spans.list()) {
        write(v);
      }
    }
  }
  if (out.flush) {
    out.flush();  // views of the packed buffers must be consumed before `outs` is destroyed
  }
}
