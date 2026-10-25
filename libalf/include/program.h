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
#include <optional>
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

    /** The distinct strings the expression looks for (regexes excluded). */
    [[nodiscard]] auto literals() const -> std::vector<std::string> const& { return m_literals; }

    /**
     * The answer for a line that contains none of literals(), which is the same for every such
     * line.  Empty when that isn't knowable: the expression has a regex (it could match a line
     * with no literal in it) or an empty string (every line contains it).
     */
    [[nodiscard]] auto answer_without_literals() const -> std::optional<bool> { return m_without_literals; }

    /**
     * A set of strings at least one of which is on every line that passes, or nothing if no such
     * set exists (an expression that can be true of a line with none of its strings, like
     * !'a').  Where there's a choice (either side of an &), the side with the lower `cost` wins.
     * A regex contributes the fixed text it starts with: r'code=5\d\d' requires "code=5".
     */
    [[nodiscard]] auto required_literals(std::function<double(std::string_view)> const& cost) const
      -> std::optional<std::vector<std::string>>;

  private:
    struct Op
    {
      types::TYPE_TOKEN type;
      bool required;
      std::string literal;
      std::shared_ptr<std::regex const> regex;
    };

    // The postfix ops as a tree, so evaluation can stop early: in 'a' & 'b', a line without 'a'
    // never searches for 'b'.  Leaves point at their Op; operators at their operands.
    struct Node
    {
      types::TYPE_TOKEN type;
      std::size_t op;     // leaves: index into m_ops
      std::size_t left;   // operators: operand indices into m_nodes (NOT uses `left` only)
      std::size_t right;
    };

    [[nodiscard]] auto eval(std::size_t node, std::string_view line) const -> bool;

    std::vector<Op> m_ops;
    std::vector<Node> m_nodes;  // the root is the last node
    std::size_t m_depth{ 0 };   // deepest the evaluation stack gets
    std::vector<std::string> m_literals;
    std::optional<bool> m_without_literals;
  };

  /** Where filter() sends the kept text. */
  struct Output
  {
    std::function<void(std::string_view)> write;  // one piece of kept text, in order
    std::function<void()> flush;                  // optional: consume every piece received so far
  };

  /**
   * Filter the complete lines in `data` and pass the kept ones, line endings included and in
   * their original order, to `out.write` (possibly in several pieces).  A last line without a
   * trailing '\n' is treated as a line.  Lines are matched without their "\n" or "\r\n".
   *
   * Lifetimes: most pieces are views into `data` and stay valid as long as `data` does.  With
   * several threads, a thread whose kept lines are many short scattered pieces packs them into
   * a buffer of its own and passes a view of that; such a view is valid only until `out.flush`
   * is called, which filter() always does before releasing the buffer.  A sink that consumes
   * each piece inside `write` can leave `flush` empty; one that collects views (to write them
   * in batches) must write them out in `flush`.
   *
   * When the expression requires one of a few strings (see Program::required_literals), or when
   * every line without its strings gets the same answer, the buffer is searched for those
   * strings instead of being read line by line: only lines with a hit are evaluated, and the
   * lines in between are skipped or kept in one piece.  A 64 KB sample decides: if more than
   * `max_hit_fraction` of its lines would be hits, reading every line is cheaper.
   *
   * With threads > 1 and enough data, the buffer is cut at line breaks into one piece per
   * thread; each piece is filtered on its own thread and the results are written in order.
   */
  void filter(std::string_view data, Program const& program, unsigned threads, Output const& out,
              double max_hit_fraction = 0.25);

  /**
   * The fixed text every match of `pattern` (ECMAScript) starts with, or "" if there is none
   * that is safe to rely on: r'code=5\d\d' -> "code=5", r'ab*c' -> "a", r'a|b' -> "".
   */
  [[nodiscard]] auto regex_required_prefix(std::string_view pattern) -> std::string;
}
