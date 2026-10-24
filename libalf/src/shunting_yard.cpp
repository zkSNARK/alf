//
// Created by Christopher Goebel on 4/2/20.
//

#include "shunting_yard.h"

#include <vector>
#include <cstdint>
#include <stack>
#include <stdexcept>


namespace
{

  auto companion_parenthesis_type(alf::types::TYPE_TOKEN tt) -> alf::types::TYPE_TOKEN
  {
    switch (tt) {
      case alf::types::TYPE_TOKEN::BRACKET_OPEN_CURLY :
        return alf::types::TYPE_TOKEN::BRACKET_CLOSE_CURLY;
      case alf::types::TYPE_TOKEN::BRACKET_CLOSE_CURLY :
        return alf::types::TYPE_TOKEN::BRACKET_OPEN_CURLY;

      case alf::types::TYPE_TOKEN::BRACKET_OPEN_PAREN:
        return alf::types::TYPE_TOKEN::BRACKET_CLOSE_PAREN;
      case alf::types::TYPE_TOKEN::BRACKET_CLOSE_PAREN:
        return alf::types::TYPE_TOKEN::BRACKET_OPEN_PAREN;

      case alf::types::TYPE_TOKEN::BRACKET_OPEN_SQUARE:
        return alf::types::TYPE_TOKEN::BRACKET_CLOSE_SQUARE;
      case alf::types::TYPE_TOKEN::BRACKET_CLOSE_SQUARE:
        return alf::types::TYPE_TOKEN::BRACKET_OPEN_SQUARE;
      default:
        throw std::runtime_error("Unknown type passed to 'auto companion_parenthesis_type(alf::types::TYPE_TOKEN tt) "
                                 "-> alf::types::TYPE_TOKEN'");
    }
  }


  auto precedence(alf::types::TYPE_TOKEN const& tt) -> uint8_t
  {
    switch (tt) {
      case alf::types::TYPE_TOKEN::SUBSTR :
        return 0;
      case alf::types::TYPE_TOKEN::OPERATOR_OR:
        return 10;
      case alf::types::TYPE_TOKEN::OPERATOR_XOR:
        return 20;
      case alf::types::TYPE_TOKEN::OPERATOR_AND:
        return 30;
      case alf::types::TYPE_TOKEN::OPERATOR_NOT:
        return 40;
      case alf::types::TYPE_TOKEN::BRACKET_OPEN_CURLY:
      case alf::types::TYPE_TOKEN::BRACKET_CLOSE_CURLY:
      case alf::types::TYPE_TOKEN::BRACKET_OPEN_PAREN:
      case alf::types::TYPE_TOKEN::BRACKET_CLOSE_PAREN:
      case alf::types::TYPE_TOKEN::BRACKET_OPEN_SQUARE:
      case alf::types::TYPE_TOKEN::BRACKET_CLOSE_SQUARE:
        return 50;
      default:
        throw std::runtime_error("Unknown type passed to 'auto precedence(alf::types::TYPE_TOKEN const& tt) "
                                 "-> uint8_t'");
    }
  }

  auto is_bracket(alf::types::TYPE_TOKEN tt) -> bool
  {
    return alf::types::is_opening_bracket(tt) or alf::types::is_closing_bracket(tt);
  }

}


/**
 * Infix to postfix.  Binary operators of equal precedence are kept right-associative
 * (a & b & c -> a b c & &); that is safe because and, or and xor are all associative.
 * The unary NOT binds tightest and is only ever written in front of a group.
 */
auto alf::shunting_yard(std::vector<alf::types::TokenBase> v) -> std::vector<alf::types::TokenBase>
{
  std::stack<alf::types::TokenBase> stk;
  std::vector<alf::types::TokenBase> que;
  que.reserve(v.size());
  bool previous_was_opening_bracket = false;

  for (auto& t : v) {
    bool const is_open = is_opening_bracket(t.type);
    if (is_operator(t.type)) {
      if (!is_unary_operator(t.type)) {
        // pop operators that bind tighter, stopping at the group's opening bracket
        int p = precedence(t.type);
        while (!stk.empty() and !is_opening_bracket(stk.top().type) and p < precedence(stk.top().type)) {
          que.emplace_back(std::move(stk.top()));
          stk.pop();
        }
      }
      stk.push(std::move(t));
    } else if (is_bracket(t.type)) {
      if (is_closing_bracket(t.type) and previous_was_opening_bracket) {
        throw std::runtime_error("empty group");
      }
      if (is_opening_bracket(t.type)) {
        stk.push(std::move(t));
      } else {
        while (!stk.empty() and !is_opening_bracket(stk.top().type)) {
          que.emplace_back(std::move(stk.top()));
          stk.pop();
        }
        if (stk.empty() or companion_parenthesis_type(stk.top().type) != t.type) {
          throw std::runtime_error("imbalanced parenthesis detected");
        }
        stk.pop();
      }
    } else {
      que.emplace_back(std::move(t));
    }
    previous_was_opening_bracket = is_open;
  }
  while (!stk.empty()) {
    if (is_opening_bracket(stk.top().type)) {
      throw std::runtime_error("imbalanced parenthesis detected");
    }
    que.emplace_back(std::move(stk.top()));
    stk.pop();
  }

  return que;
}
