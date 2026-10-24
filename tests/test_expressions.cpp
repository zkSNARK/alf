//
// End-to-end tests: expression text -> parse -> implied ANDs -> postfix -> filter a line.
//

#include "line_filter.h"
#include "parser.h"
#include "shunting_yard.h"

#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>


namespace
{
  auto compile(std::string const& expression) -> std::vector<alf::types::TokenBase>
  {
    auto tokens = alf::parser::parse_algebraic(expression);
    tokens = alf::parser::fill_in_missing_AND_symbols(tokens);
    tokens = alf::shunting_yard(std::move(tokens));
    (void) alf::passes_filters(tokens, "");  // what main() does: reject malformed input up front
    return tokens;
  }

  auto matches(std::string const& expression, std::string const& line) -> bool
  {
    return alf::passes_filters(compile(expression), line);
  }


  TEST(Expressions, bang_negates_a_string)
  {
    EXPECT_TRUE(matches("('error' | 'warn') & !'heartbeat'", "error x"));
    EXPECT_FALSE(matches("('error' | 'warn') & !'heartbeat'", "warn heartbeat"));
    EXPECT_FALSE(matches("!'a'", "a"));
    EXPECT_TRUE(matches("!'a'", "b"));
  }

  TEST(Expressions, minus_and_bang_are_the_same)
  {
    for (std::string line : { "a", "b", "ab", "" }) {
      EXPECT_EQ(matches("-'a' | 'b'", line), matches("!'a' | 'b'", line)) << line;
    }
  }

  TEST(Expressions, double_negation_cancels)
  {
    EXPECT_TRUE(matches("!!'a'", "a"));
    EXPECT_FALSE(matches("--'a'", "b"));
  }

  TEST(Expressions, and_binds_tighter_than_or)
  {
    // 'a' & 'b' | 'c' used to crash: the shunting yard popped the open bracket around the
    // expression into the output.
    EXPECT_TRUE(matches("'a' & 'b' | 'c'", "c"));
    EXPECT_TRUE(matches("'a' & 'b' | 'c'", "ab"));
    EXPECT_FALSE(matches("'a' & 'b' | 'c'", "a"));
    EXPECT_TRUE(matches("'c' | 'a' & 'b'", "c"));
    EXPECT_FALSE(matches("'c' | 'a' & 'b'", "b"));
  }

  TEST(Expressions, xor_sits_between_and_and_or)
  {
    // a | b ^ c & d  ==  a | (b ^ (c & d))
    EXPECT_TRUE(matches("'a' | 'b' ^ 'c' & 'd'", "b"));
    EXPECT_FALSE(matches("'a' | 'b' ^ 'c' & 'd'", "bcd"));
    EXPECT_TRUE(matches("'a' | 'b' ^ 'c' & 'd'", "cd"));
    EXPECT_TRUE(matches("'a' | 'b' ^ 'c' & 'd'", "abcd"));
  }

  TEST(Expressions, negated_group)
  {
    EXPECT_TRUE(matches("-('a' | 'b')", "c"));
    EXPECT_FALSE(matches("-('a' | 'b')", "b"));
    EXPECT_TRUE(matches("!('a' & 'b')", "a"));
    EXPECT_FALSE(matches("!('a' & 'b')", "ab"));
    EXPECT_TRUE(matches("'x' !['a' | {'b' & !'c'}]", "x bc"));
    EXPECT_FALSE(matches("'x' !['a' | {'b' & !'c'}]", "x b"));
  }

  TEST(Expressions, implied_and)
  {
    EXPECT_TRUE(matches("'a' 'b'", "ab"));
    EXPECT_FALSE(matches("'a' 'b'", "a"));
    EXPECT_TRUE(matches("'a' ('b' | 'c')", "ac"));
    EXPECT_FALSE(matches("'a' !('b' | 'c')", "ac"));
    EXPECT_TRUE(matches("('a') !('b')", "a"));
  }

  TEST(Expressions, readme_examples)
  {
    std::string const e = "('a' & !'b') | ('a' & (('b' | 'c') & 'd'))";
    EXPECT_TRUE(matches(e, "a"));
    EXPECT_FALSE(matches(e, "ab"));
    EXPECT_TRUE(matches(e, "abd"));
    EXPECT_FALSE(matches(e, "d"));
  }

  TEST(Expressions, malformed_input_is_an_error_not_a_crash)
  {
    for (std::string bad : { "('a'", "'a')", "('a']", "'a' &", "& 'a'", "'a' -", "-)", "'a' & | 'b'", "()", "!" }) {
      EXPECT_THROW(compile(bad), std::runtime_error) << bad;
    }
  }

  TEST(Expressions, regex_matches_anywhere_in_the_line)
  {
    EXPECT_TRUE(matches("r'code=\\d+'", "error code=503 upstream"));
    EXPECT_FALSE(matches("r'code=\\d+'", "error code=abc"));
    EXPECT_TRUE(matches("r\"^\\[(WARN|ERROR)\\]\"", "[WARN] disk 91%"));
    EXPECT_FALSE(matches("r\"^\\[(WARN|ERROR)\\]\"", "info [WARN] quoted"));
    EXPECT_TRUE(matches("r'done$'", "job 7 done"));
    EXPECT_FALSE(matches("r'done$'", "done with job 7"));
  }

  TEST(Expressions, regex_negation_and_operators)
  {
    EXPECT_TRUE(matches("-r'\\d'", "no digits here"));
    EXPECT_FALSE(matches("!r'\\d'", "4 digits"));
    EXPECT_TRUE(matches("'error' & !r'timeout|reset'", "error: bad request"));
    EXPECT_FALSE(matches("'error' & !r'timeout|reset'", "error: connection reset"));
    EXPECT_TRUE(matches("r'^a' 'z'", "abcz"));            // implied & between operands
    EXPECT_FALSE(matches("r'^a' 'z'", "abc"));
    EXPECT_TRUE(matches("'x' & r'^y' | r'z$'", "abz"));   // & before |, regexes included
    EXPECT_TRUE(matches("!(r'^a' | r'b$')", "cac"));
  }

  TEST(Expressions, r_is_only_special_right_against_a_quote)
  {
    EXPECT_TRUE(matches("r 'x'", "r x"));    // the substring "r", then 'x'
    EXPECT_FALSE(matches("r 'x'", "x"));
    EXPECT_TRUE(matches("'r'", "bar"));
    EXPECT_TRUE(matches("bar'x'", "bar x")); // a word ending in r is still a word
  }

  TEST(Expressions, regex_tokens_from_the_parser)
  {
    auto tokens = alf::parser::parse_algebraic("-r'a.c'");
    ASSERT_EQ(tokens.size(), 1u);
    EXPECT_EQ(tokens[0].type, alf::types::TYPE_TOKEN::REGEX);
    EXPECT_EQ(tokens[0].value, "a.c");
    EXPECT_FALSE(tokens[0].required);
  }

  TEST(Expressions, invalid_regex_is_an_error)
  {
    EXPECT_THROW(compile("r'(unclosed'"), std::runtime_error);
    EXPECT_THROW(compile("r'[z-a]'"), std::runtime_error);
  }

  TEST(Expressions, no_requirements_passes_everything)
  {
    EXPECT_TRUE(alf::passes_filters({}, "anything"));
  }


  // ---- randomized check against a reference evaluator ------------------------------------

  struct Node
  {
    char op;                 // 'v' leaf, '!' not, '&', '^', '|'
    char var;
    std::unique_ptr<Node> l, r;
  };

  auto precedence(char op) -> int
  {
    switch (op) {
      case '|': return 1;
      case '^': return 2;
      case '&': return 3;
      default: return 4;
    }
  }

  auto random_tree(std::mt19937& rng, int depth) -> std::unique_ptr<Node>
  {
    auto n = std::make_unique<Node>();
    if (depth == 0 or rng() % 4 == 0) {
      n->op = 'v';
      n->var = static_cast<char>('a' + rng() % 4);
      return n;
    }
    static char const ops[] = { '&', '^', '|', '!' };
    n->op = ops[rng() % 4];
    n->l = random_tree(rng, depth - 1);
    if (n->op != '!') {
      n->r = random_tree(rng, depth - 1);
    }
    return n;
  }

  // Fewest brackets precedence allows; every operator is associative, so equal precedence needs
  // none.  Bracket shapes rotate to exercise all three.
  auto render(Node const& n, int& shape) -> std::string
  {
    static std::string const open = "([{", close = ")]}";
    auto group = [&](std::string const& s) {
      int k = shape++ % 3;
      return open[k] + s + close[k];
    };
    if (n.op == 'v') {
      return std::string("'") + n.var + "'";
    }
    if (n.op == '!') {
      std::string inner = render(*n.l, shape);
      return n.l->op == 'v' ? "!" + inner : "!" + group(inner);
    }
    auto side = [&](Node const& c) {
      std::string s = render(c, shape);
      return (c.op != 'v' and c.op != '!' and precedence(c.op) < precedence(n.op)) ? group(s) : s;
    };
    return side(*n.l) + " " + n.op + " " + side(*n.r);
  }

  auto eval(Node const& n, std::string const& line) -> bool
  {
    switch (n.op) {
      case 'v': return line.find(n.var) != std::string::npos;
      case '!': return !eval(*n.l, line);
      case '&': return eval(*n.l, line) and eval(*n.r, line);
      case '^': return eval(*n.l, line) != eval(*n.r, line);
      default:  return eval(*n.l, line) or eval(*n.r, line);
    }
  }

  TEST(Expressions, random_expressions_match_a_reference_evaluator)
  {
    std::mt19937 rng(20260424);
    for (int i = 0; i < 3000; ++i) {
      auto tree = random_tree(rng, 5);
      int shape = 0;
      std::string expression = render(*tree, shape);
      auto compiled = compile(expression);
      for (int bits = 0; bits < 16; ++bits) {
        std::string line;
        for (int b = 0; b < 4; ++b) {
          if (bits & (1 << b)) {
            line += static_cast<char>('a' + b);
          }
        }
        ASSERT_EQ(alf::passes_filters(compiled, line), eval(*tree, line))
          << "expression: " << expression << "\nline: '" << line << "'";
      }
    }
  }
}
