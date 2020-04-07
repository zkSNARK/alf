//
// Created by Christopher Goebel on 4/6/20.
//

#include "line_filter.h"

#include <stack>
#include <stdexcept>

namespace {
  template<typename T, typename Container=std::deque<T> >
  class iterable_queue : public std::queue<T,Container>
  {
  public:
    typedef typename Container::iterator iterator;
    typedef typename Container::const_iterator const_iterator;

    std::queue<T> q_;

    explicit iterable_queue(std::queue<T>q)
      : std::queue<T>(q)
    {}

    iterator begin() { return this->c.begin(); }
    iterator end() { return this->c.end(); }
    [[nodiscard]] const_iterator begin() const { return this->c.begin(); }
    [[nodiscard]] const_iterator end() const { return this->c.end(); }
  };
}


auto alf::handle_operator(std::stack<bool>& stk, types::TYPE_TOKEN tt) -> void
{
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
    handle_operator(stk, e.type);
  }
  return stk.top();
}
