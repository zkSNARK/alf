//
// Created by Christopher Goebel on 4/2/20.
//

#include <vector>
#include <queue>
#include <stack>
#include <string>
#include <stdexcept>

auto is_opening_paren(char c) -> bool {
  return c == '(' or c == '{' or c == '[';
}

auto is_closing_paren(char c) -> bool {
  return c == ')' or c == '}' or c == ']';
}

auto companion_parenthesis(char c) -> char {
  switch (c) {
    case '{' :return '}';
    case '}':return '{';
    case '(':return ')';
    case ')':return '(';
    case '[':return ']';
    case ']':return '[';
    default:throw std::runtime_error("unknown symbol in 'companion_parenthesis' : " + std::string{c});
  }
}

auto is_operator(std::string const &s) -> bool {
  return s == "+" or s == "-" or s == "&" or s == "|";
}

auto is_parenthesis(std::string const &s) -> bool {
  return s.size() == 1 and (s == "(" or s == ")" or s == "{" or s == "}" or s == "[" or s == "]");
}

auto precedence(char c) -> int {
  switch (c) {
    case '+':
    case '-':return 2;
    case '|':return 3;
    case '&':return 4;
    default:throw std::runtime_error("invalid character detected in 'precedence' : " + std::string{c});
  }
}

auto shunting_yard(std::vector<std::string> v) -> std::queue<std::string>
{
  std::stack<char> stk;
  std::queue<std::string> que;

  for (auto &s : v) {
    if (is_operator(s)) {
      int p = precedence(s[0]);
      if (stk.empty() or is_opening_paren(stk.top()) or p >= precedence(stk.top())) {
        stk.push(s[0]);
        continue;
      }
      while (p < precedence(stk.top())) {
        que.push({stk.top()});
        stk.pop();
      }
      stk.push(s[0]);
    } else if (is_parenthesis(s)) {
      char c = s[0];
      if (is_opening_paren(c)) {
        stk.push(c);
      } else {
        while (!is_opening_paren(stk.top())) {
          que.push({stk.top()});
          stk.pop();
        }
        if (char cp = companion_parenthesis(stk.top()); cp != c) {
          throw std::runtime_error("imbalanced parenthesis detected");
        }
        stk.pop();
      }
    } else {
      que.push(std::move(s));
    }
  }
  while (!stk.empty()) {
    que.push({stk.top()});
    stk.pop();
  }

  return std::move(que);
}