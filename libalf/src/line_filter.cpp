//
// Created by Christopher Goebel on 4/6/20.
//

#include "line_filter.h"
#include "unicode/unicode_tables.h"

#include <regex>
#include <stack>
#include <stdexcept>


auto alf::handle_operator(std::stack<bool>& stk, types::TYPE_TOKEN tt) -> void
{
  if (types::is_unary_operator(tt)) {
    if (stk.empty()) {
      throw std::runtime_error("malformed expression: nothing to negate");
    }
    stk.top() = !stk.top();
    return;
  }
  if (stk.size() < 2) {
    throw std::runtime_error("malformed expression: an operator is missing an operand");
  }

  bool a = stk.top();
  stk.pop();
  bool b = stk.top();
  stk.pop();

  switch (tt) {
    case types::TYPE_TOKEN::OPERATOR_AND: {
      stk.push(a and b);
      break;
    }
    case types::TYPE_TOKEN::OPERATOR_OR: {
      stk.push(a or b);
      break;
    }
    case types::TYPE_TOKEN::OPERATOR_XOR: {
      stk.push(a xor b);
      break;
    }
    default:
      throw std::runtime_error("invalid token found in auto handle_operator(std::stack<bool>& stk, types::TokenBase const& t) -> void");
  }
}



namespace
{
  // The reference rules for i'...' and w'...', written as plainly as possible (the line decoded
  // into characters front to back, every table searched entry by entry): the fast paths in
  // program.cpp, which do it differently, are tested against these.

  // Word characters as Unicode defines them, looked up the slow way (every range in turn).
  auto is_word_char(char32_t c) -> bool
  {
    for (auto const& r : alf::unicode::word_ranges) {
      if (c >= r.lo and c <= r.hi) {
        return true;
      }
    }
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_';
  }

  // The line as characters: each one's first byte, its length, and its code point, or 0xFFFFFFFF
  // for a byte that doesn't start a well-formed UTF-8 character (taken one byte at a time).
  struct Char
  {
    std::size_t at, len;
    char32_t c;
  };

  auto characters(std::string const& s) -> std::vector<Char>
  {
    std::vector<Char> out;
    for (std::size_t i = 0; i < s.size();) {
      auto const b = static_cast<unsigned char>(s[i]);
      std::size_t const n = b < 0x80 ? 1 : (b >> 5) == 6 ? 2 : (b >> 4) == 14 ? 3 : (b >> 3) == 30 ? 4 : 0;
      char32_t c = 0xFFFFFFFF;
      if (n == 1) {
        c = b;
      } else if (n > 1 and i + n <= s.size()) {
        char32_t v = b & (0xFF >> (n + 1));
        bool ok = true;
        for (std::size_t k = 1; k < n; ++k) {
          auto const x = static_cast<unsigned char>(s[i + k]);
          ok = ok and (x >> 6) == 2;
          v = (v << 6) | (x & 0x3F);
        }
        char32_t const min = n == 2 ? 0x80 : n == 3 ? 0x800 : 0x10000;
        if (ok and v >= min and v <= 0x10FFFF and !(v >= 0xD800 and v <= 0xDFFF)) {
          c = v;
        }
      }
      std::size_t const len = c == 0xFFFFFFFF ? 1 : n;
      out.push_back({ i, len, c });
      i += len;
    }
    return out;
  }

  // Simple case folding; a byte that isn't a well-formed character (0xFFFFFFFF here) is only
  // ever equal to the same byte.
  auto fold_char(char32_t c) -> char32_t
  {
    for (auto const& f : alf::unicode::fold_table) {
      if (f.from == c) {
        return f.to;
      }
    }
    return c;
  }

  // Is there a word character ending exactly at byte `pos` (before) / starting exactly there (after)?
  auto word_ends_at(std::vector<Char> const& cs, std::size_t pos) -> bool
  {
    for (auto const& ch : cs) {
      if (ch.at + ch.len == pos) {
        return ch.c != 0xFFFFFFFF and is_word_char(ch.c);
      }
    }
    return false;
  }

  auto word_starts_at(std::vector<Char> const& cs, std::size_t pos) -> bool
  {
    for (auto const& ch : cs) {
      if (ch.at == pos) {
        return ch.c != 0xFFFFFFFF and is_word_char(ch.c);
      }
    }
    return false;
  }

  auto contains(std::string const& line, alf::types::TokenBase const& t) -> bool
  {
    std::string const& s = t.value;
    std::vector<Char> const cs = characters(line);
    if (t.nocase) {
      // compare characters after folding, from every character of the line on
      std::vector<Char> const want = characters(s);
      auto const key = [&](std::string const& text, Char const& ch) -> std::pair<char32_t, unsigned char> {
        return ch.c == 0xFFFFFFFF ? std::pair<char32_t, unsigned char>{ 0xFFFFFFFF, static_cast<unsigned char>(text[ch.at]) }
                                  : std::pair<char32_t, unsigned char>{ fold_char(ch.c), 0 };
      };
      for (std::size_t i = 0; i + want.size() <= cs.size(); ++i) {
        bool same = true;
        for (std::size_t k = 0; k < want.size() and same; ++k) {
          same = key(line, cs[i + k]) == key(s, want[k]);
        }
        std::size_t const from = i < cs.size() ? cs[i].at : line.size();
        std::size_t const to = i + want.size() < cs.size() ? cs[i + want.size()].at : line.size();
        if (same and (!t.word or (!word_ends_at(cs, from) and !word_starts_at(cs, to)))) {
          return true;
        }
      }
      return false;
    }
    if (s.size() > line.size()) {
      return false;
    }
    for (std::size_t at = 0; at + s.size() <= line.size(); ++at) {
      bool same = line.compare(at, s.size(), s) == 0;
      if (same and t.word) {
        same = !word_ends_at(cs, at) and !word_starts_at(cs, at + s.size());
      }
      if (same) {
        return true;
      }
    }
    return false;
  }
}


auto alf::passes_filters(std::vector<alf::types::TokenBase> const& filters, const std::string& line) -> bool
{
  using alf::types::TokenBase;

  if (filters.empty()) {
    return true;  // no requirements: every line passes
  }

  std::stack<bool> stk;

  for (auto & e : filters) {
    if (e.type == types::TYPE_TOKEN::SUBSTR) {
      bool const found = (e.nocase or e.word) ? contains(line, e) : line.find(e.value) != std::string::npos;
      stk.push(found == e.required);
      continue;
    }
    if (e.type == types::TYPE_TOKEN::REGEX) {
      stk.push(std::regex_search(line, *e.regex) == e.required);
      continue;
    }
    handle_operator(stk, e.type);
  }
  if (stk.size() != 1) {
    throw std::runtime_error("malformed expression: operands without an operator between them");
  }
  return stk.top();
}
