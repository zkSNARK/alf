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



auto alf::passes_filters(std::vector<alf::types::TokenBase> const& filters, const std::string& line) -> bool
{
  using alf::types::TokenBase;

  if (filters.empty()) {
    return true;  // no requirements: every line passes
  }

  std::stack<bool> stk;

  for (auto & e : filters) {
    if (e.type == types::TYPE_TOKEN::SUBSTR) {
      std::size_t found = line.find(e.value);
      if(e.required) {
        stk.push(found != std::string::npos);
      } else {
        stk.push(found == std::string::npos);
      }
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
