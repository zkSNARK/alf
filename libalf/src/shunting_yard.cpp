//
// Created by Christopher Goebel on 4/2/20.
//

#include "alf_types.h"

#include <vector>
#include <queue>
#include <stack>
#include <string>
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
      case alf::types::TYPE_TOKEN::OPERATOR_XOR:
        return 10;
      case alf::types::TYPE_TOKEN::OPERATOR_AND:
        return 20;
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

auto shunting_yard(std::vector<alf::types::TokenBase> v) -> std::queue<alf::types::TokenBase>
{
  std::stack<alf::types::TokenBase> stk;
  std::queue<alf::types::TokenBase> que;

  for (auto& t : v) {
    if (is_operator(t.type)) {
      int p = precedence(t.type);
      if (stk.empty() or is_opening_bracket(stk.top().type) or p >= precedence(stk.top().type)) {
        stk.push(std::move(t));
        continue;
      }
      while (p < precedence(stk.top().type)) {
        que.push(std::move(stk.top()));
        stk.pop();
      }
      stk.push(t);
    } else if (is_bracket(t.type)) {
      if (is_opening_bracket(t.type)) {
        stk.push(std::move(t));
      } else {
        while (!is_opening_bracket(stk.top().type)) {
          que.push(std::move(stk.top()));
          stk.pop();
        }
        if (companion_parenthesis_type(stk.top().type) != t.type) {
          throw std::runtime_error("imbalanced parenthesis detected");
        }
        stk.pop();
      }
    } else {
      que.push(std::move(t));
    }
  }
  while (!stk.empty()) {
    que.push({ stk.top() });
    stk.pop();
  }

  return std::move(que);
}
