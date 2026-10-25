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

  auto line_end(char const* p, char const* end) -> char const*
  {
    auto nl = static_cast<char const*>(std::memchr(p, '\n', static_cast<std::size_t>(end - p)));
    return nl ? nl : end;
  }

  // Filter [first, last) line by line, appending kept lines to `out`.
  void filter_range(char const* first, char const* last, alf::Program const& program, std::string& out)
  {
    for (char const* p = first; p < last;) {
      char const* e = line_end(p, last);
      char const* text_end = (e > p and e[-1] == '\r') ? e - 1 : e;
      char const* next = e < last ? e + 1 : e;
      if (program.matches(std::string_view(p, static_cast<std::size_t>(text_end - p)))) {
        out.append(p, next);
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
}


auto alf::Program::matches(std::string_view line) const -> bool
{
  if (m_ops.empty()) {
    return true;
  }
  // one stack per thread, sized once: no allocation per line
  thread_local std::vector<char> stack;
  if (stack.size() < m_depth) {
    stack.resize(m_depth);
  }
  char* stk = stack.data();
  std::size_t sp = 0;
  for (auto const& op : m_ops) {
    switch (op.type) {
      case types::TYPE_TOKEN::SUBSTR:
        stk[sp++] = (line.find(op.literal) != std::string_view::npos) == op.required;
        break;
      case types::TYPE_TOKEN::REGEX:
        stk[sp++] = std::regex_search(line.begin(), line.end(), *op.regex) == op.required;
        break;
      case types::TYPE_TOKEN::OPERATOR_NOT:
        stk[sp - 1] = !stk[sp - 1];
        break;
      case types::TYPE_TOKEN::OPERATOR_AND:
        --sp;
        stk[sp - 1] = stk[sp - 1] and stk[sp];
        break;
      case types::TYPE_TOKEN::OPERATOR_OR:
        --sp;
        stk[sp - 1] = stk[sp - 1] or stk[sp];
        break;
      case types::TYPE_TOKEN::OPERATOR_XOR:
        --sp;
        stk[sp - 1] = stk[sp - 1] != stk[sp];
        break;
      default:
        break;
    }
  }
  return stk[0];
}


void alf::filter(std::string_view data, Program const& program, unsigned threads,
                 std::function<void(std::string_view)> const& write)
{
  char const* begin = data.data();
  char const* end = begin + data.size();
  threads = std::max(1u, std::min<unsigned>(threads, static_cast<unsigned>(data.size() / MIN_BYTES_PER_THREAD)));

  if (threads == 1) {
    std::string out;
    out.reserve(std::min<std::size_t>(data.size(), 1 << 20));
    filter_range(begin, end, program, out);
    write(out);
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

  std::vector<std::string> outs(threads);
  std::vector<std::exception_ptr> errors(threads);
  std::vector<std::thread> pool;
  pool.reserve(threads);
  for (unsigned t = 0; t < threads; ++t) {
    pool.emplace_back([&, t] {
      try {
        filter_range(cuts[t], cuts[t + 1], program, outs[t]);
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
    write(o);
  }
}
