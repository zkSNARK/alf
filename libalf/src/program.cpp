//
// program.cpp
//

#include "program.h"

#include "simd.h"

#include <algorithm>
#include <array>
#include <cstdint>
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

  // A string to search for, and which of its bytes is rarest in the input: memchr finds that
  // byte (it checks many bytes per instruction), then memcmp checks the rest of the string.
  // This is the same trick grep uses, and on this kind of data it is several times faster than
  // memmem or Boyer-Moore-Horspool.
  struct Needle
  {
    std::string_view text;
    std::size_t rare;
  };

  auto make_needles(std::vector<std::string> const& literals, std::string_view sample) -> std::vector<Needle>
  {
    std::size_t freq[256] = {};
    for (unsigned char c : sample) {
      ++freq[c];
    }
    std::vector<Needle> needles;
    for (auto const& lit : literals) {
      std::size_t rare = 0;
      for (std::size_t i = 1; i < lit.size(); ++i) {
        if (freq[static_cast<unsigned char>(lit[i])] < freq[static_cast<unsigned char>(lit[rare])]) {
          rare = i;
        }
      }
      needles.push_back({ lit, rare });
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
      q = static_cast<char const*>(std::memchr(q, key, static_cast<std::size_t>(q_end - q)));
      if (q == nullptr) {
        return last;
      }
      char const* start = q - n.rare;
      if (std::memcmp(start, n.text.data(), n.text.size()) == 0) {
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
  //  - one string: memchr for its rare byte (the C library already vectorizes memchr);
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
      m_set.count = 0;
      for (bool& b : m_set.member) {
        b = false;
      }
      for (std::size_t i = 0; i < needles.size(); ++i) {
        auto const key = static_cast<unsigned char>(needles[i].text[needles[i].rare]);
        if (!m_set.member[key]) {
          m_set.member[key] = true;
          m_set.bytes[m_set.count++] = key;
        }
        m_by_key[key].push_back(static_cast<std::uint16_t>(i));
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
      if (m_set.count == 1) {
        auto q = static_cast<char const*>(std::memchr(p, m_set.bytes[0], static_cast<std::size_t>(last - p)));
        return q ? q : last;
      }
      return m_simd.find_any(m_set, p, last, limit);
    }

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
            std::memcmp(start, n.text.data(), n.text.size()) == 0) {
          on_hit(i);
        }
      };
      if (m_teddy) {
        for (unsigned g = groups_at(q, last); g != 0; g &= g - 1) {
          for (std::uint16_t i : m_groups[static_cast<std::size_t>(__builtin_ctz(g))]) {
            check(i, m_window[i]);
          }
        }
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
        for (std::size_t o = 0; o + 3 <= t.size(); ++o) {
          double const c = freq[static_cast<unsigned char>(t[o])] * freq[static_cast<unsigned char>(t[o + 1])] *
                           freq[static_cast<unsigned char>(t[o + 2])];
          if (best_cost < 0 or c < best_cost) {
            best_cost = c;
            best = o;
          }
        }
        m_window[i] = best;
        fp[i] = t.substr(best, 3);
      }
      // similar fingerprints share a group, so a group's combined tables let fewer bytes through
      std::vector<std::uint16_t> order(m_needles.size());
      for (std::size_t i = 0; i < order.size(); ++i) {
        order[i] = static_cast<std::uint16_t>(i);
      }
      std::sort(order.begin(), order.end(), [&](std::uint16_t a, std::uint16_t b) { return fp[a] < fp[b]; });
      m_tables = {};
      for (std::size_t r = 0; r < order.size(); ++r) {
        std::size_t const g = r * 8 / order.size();
        std::uint16_t const i = order[r];
        m_groups[g].push_back(i);
        auto const bit = static_cast<unsigned char>(1u << g);
        for (std::size_t k = 0; k < 3; ++k) {
          if (k < fp[i].size()) {
            auto const b = static_cast<unsigned char>(fp[i][k]);
            m_tables.lo[k][b & 15u] |= bit;
            m_tables.hi[k][b >> 4] |= bit;
          } else {  // a string shorter than 3 bytes: any byte matches here
            for (int n = 0; n < 16; ++n) {
              m_tables.lo[k][n] |= bit;
              m_tables.hi[k][n] |= bit;
            }
          }
        }
      }
    }

    std::vector<Needle> const& m_needles;
    alf::simd::Backend const& m_simd;
    // rare-byte mode
    alf::simd::ByteSet m_set{};
    std::array<std::vector<std::uint16_t>, 256> m_by_key;
    // Teddy mode
    bool m_teddy{ false };
    alf::simd::TeddyTables m_tables{};
    std::array<std::vector<std::uint16_t>, 8> m_groups;
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
  void filter_range_search(char const* first, char const* last, alf::Program const& program,
                           MultiScanner const& scan, bool keep_others, bool use_found, Spans& out)
  {
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
      std::uint64_t found = 0;
      bool hit = false;
      scan.verify(q, cursor, last, [&](std::size_t i) {
        hit = true;
        found |= std::uint64_t{ 1 } << i;
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
        std::optional<bool> answer = program.settled(line, found);
        for (char const* c = q + 1; !answer and (c = scan.next_candidate(c, e, last)) < e; ++c) {
          std::uint64_t const before = found;
          scan.verify(c, ls, e, [&](std::size_t i) { found |= std::uint64_t{ 1 } << i; });
          if (found != before) {
            answer = program.settled(line, found);
          }
        }
        keep = answer ? *answer : program.matches_found(line, found);
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
    if (op.type == types::TYPE_TOKEN::SUBSTR and std::find(m_literals.begin(), m_literals.end(), op.literal) == m_literals.end()) {
      m_literals.push_back(op.literal);
    }
  }
  for (auto& op : m_ops) {
    if (op.type == types::TYPE_TOKEN::SUBSTR) {
      op.literal_index = static_cast<std::size_t>(std::find(m_literals.begin(), m_literals.end(), op.literal) - m_literals.begin());
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
      return (line.find(op.literal) != std::string_view::npos) == op.required;
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


auto alf::Program::matches_found(std::string_view line, std::uint64_t found) const -> bool
{
  return m_nodes.empty() or eval_found(m_nodes.size() - 1, line, found);
}


auto alf::Program::eval_found(std::size_t node, std::string_view line, std::uint64_t found) const -> bool
{
  Node const& n = m_nodes[node];
  switch (n.type) {
    case types::TYPE_TOKEN::SUBSTR: {
      Op const& op = m_ops[n.op];
      return (((found >> op.literal_index) & 1u) != 0) == op.required;
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


auto alf::Program::settled(std::string_view line, std::uint64_t found) const -> std::optional<bool>
{
  if (m_nodes.empty()) {
    return true;
  }
  return eval_settled(m_nodes.size() - 1, line, found);
}


// Three-valued (true / false / not yet known) evaluation: a string already found is known to be
// present; one not found yet might still turn up later in the line, so it is unknown.
auto alf::Program::eval_settled(std::size_t node, std::string_view line, std::uint64_t found) const
  -> std::optional<bool>
{
  Node const& n = m_nodes[node];
  switch (n.type) {
    case types::TYPE_TOKEN::SUBSTR: {
      Op const& op = m_ops[n.op];
      if (((found >> op.literal_index) & 1u) != 0) {
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
  -> std::optional<std::vector<std::string>>
{
  using Set = std::optional<std::vector<std::string>>;
  auto const total = [&](std::vector<std::string> const& v) {
    double c = 0;
    for (auto const& s : v) {
      c += cost(s);
    }
    return c;
  };
  std::vector<Set> stk;
  for (auto const& op : m_ops) {
    switch (op.type) {
      case types::TYPE_TOKEN::SUBSTR:
        stk.push_back(op.required and !op.literal.empty() ? Set{ { op.literal } } : std::nullopt);
        break;
      case types::TYPE_TOKEN::REGEX: {
        std::string prefix = op.required ? regex_required_prefix(op.literal) : std::string{};
        stk.push_back(prefix.empty() ? std::nullopt : Set{ { prefix } });
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
  std::vector<std::string> search_for;  // owns the text the needles point into
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
  // Evaluating a hit line from the strings the scanner found needs every string, at most 64 of
  // them, and none that could match across a line break (or include the "\r" a line is matched
  // without).
  bool const use_found = all_literals and needles.size() <= 64 and
    std::none_of(search_for.begin(), search_for.end(), [](std::string const& t) {
      return t.find_first_of("\r\n") != std::string::npos;
    });
  // Searching for a few required strings pays when most lines don't have them.  Searching for all
  // of the strings while recording which ones each hit line has pays even when most lines are
  // hits (it replaces one search per string per line with one pass), so it is skipped only when
  // reading every line is forced (max_hit_fraction < 0).
  if (!needles.empty() and (use_found ? max_hit_fraction < 0 : hit_fraction(needles, sample) > max_hit_fraction)) {
    needles.clear();
  }
  MultiScanner const scanner{ needles, sample };
  auto const run = [&](char const* first, char const* last, Spans& out) {
    if (needles.empty()) {
      filter_range(first, last, program, out);
    } else {
      filter_range_search(first, last, program, scanner, keep_others, use_found, out);
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
