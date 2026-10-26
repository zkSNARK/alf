//
// Created by Christopher Goebel on 4/6/20.
//

#include "line_filter.h"

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
  // The reference rules for i'...' and w'...', written as plainly as possible (every start
  // position, byte by byte): the fast paths in program.cpp are tested against these.
  auto ascii_lower(char c) -> char { return (c >= 'A' and c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

  auto is_word_byte(char c) -> bool
  {
    return (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z') or (c >= '0' and c <= '9') or c == '_';
  }

  auto contains(std::string const& line, alf::types::TokenBase const& t) -> bool
  {
    std::string const& s = t.value;
    if (s.size() > line.size()) {
      return false;
    }
    for (std::size_t at = 0; at + s.size() <= line.size(); ++at) {
      bool same = true;
      for (std::size_t k = 0; k < s.size() and same; ++k) {
        same = t.nocase ? ascii_lower(line[at + k]) == ascii_lower(s[k]) : line[at + k] == s[k];
      }
      if (same and t.word) {
        same = (at == 0 or !is_word_byte(line[at - 1])) and (at + s.size() == line.size() or !is_word_byte(line[at + s.size()]));
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
