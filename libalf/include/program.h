//
// program.h
//
// A compiled filter: the postfix token list turned into a flat program that is evaluated per
// line without allocating, plus a buffer filter that splits large inputs across threads.
//

#pragma once

#include "alf_types.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace alf
{
  class Program
  {
  public:
    /**
     * `postfix` is the output of shunting_yard().  Throws std::runtime_error if it is not a
     * well-formed expression.  An empty program keeps every line.
     */
    explicit Program(std::vector<types::TokenBase> const& postfix);

    /** Does `line` (without its line ending) pass the filter?  Safe to call from many threads. */
    [[nodiscard]] auto matches(std::string_view line) const -> bool;

  private:
    struct Op
    {
      types::TYPE_TOKEN type;
      bool required;
      std::string literal;
      std::shared_ptr<std::regex const> regex;
    };

    std::vector<Op> m_ops;
    std::size_t m_depth{ 0 };  // deepest the evaluation stack gets
  };

  /**
   * Filter the complete lines in `data` and pass the kept ones, line endings included and in
   * their original order, to `write` (possibly in several pieces).  A last line without a
   * trailing '\n' is treated as a line.  Lines are matched without their "\n" or "\r\n".
   *
   * With threads > 1 and enough data, the buffer is cut at line breaks into one piece per
   * thread; each piece is filtered on its own thread and the results are written in order.
   */
  void filter(std::string_view data, Program const& program, unsigned threads,
              std::function<void(std::string_view)> const& write);
}
