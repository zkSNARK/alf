//
// program.cpp
//

#include "program.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <stdexcept>
#include <thread>

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

  // Filter [first, last) by searching for the strings: only lines with a hit are evaluated; the
  // lines in between all get the answer `keep_others` and are skipped or kept in one piece.
  void filter_range_search(char const* first, char const* last, alf::Program const& program,
                           std::vector<Needle> const& needles, bool keep_others, Spans& out)
  {
    std::vector<char const*> next(needles.size());
    for (std::size_t i = 0; i < needles.size(); ++i) {
      next[i] = find(needles[i], first, last);
    }
    char const* cursor = first;
    while (cursor < last) {
      char const* hit = *std::min_element(next.begin(), next.end());
      if (hit == last) {
        if (keep_others) {
          out.add(cursor, last);
        }
        return;
      }
      char const* ls = hit;
      while (ls > cursor and ls[-1] != '\n') {
        --ls;
      }
      if (keep_others) {
        out.add(cursor, ls);
      }
      char const* e = line_end(hit, last);
      char const* text_end = (e > ls and e[-1] == '\r') ? e - 1 : e;
      cursor = e < last ? e + 1 : e;
      if (program.matches(std::string_view(ls, static_cast<std::size_t>(text_end - ls)))) {
        out.add(ls, cursor);
      }
      for (std::size_t i = 0; i < needles.size(); ++i) {
        if (next[i] < cursor) {
          next[i] = find(needles[i], cursor, last);
        }
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
  if (auto required = program.required_literals(cost)) {
    search_for = std::move(*required);
  } else if (auto without = program.answer_without_literals(); without and !program.literals().empty()) {
    search_for = program.literals();
    keep_others = *without;
  }
  std::vector<Needle> needles = make_needles(search_for, sample);
  if (!needles.empty() and hit_fraction(needles, sample) > max_hit_fraction) {
    needles.clear();
  }
  auto const run = [&](char const* first, char const* last, Spans& out) {
    if (needles.empty()) {
      filter_range(first, last, program, out);
    } else {
      filter_range_search(first, last, program, needles, keep_others, out);
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
