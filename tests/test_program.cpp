//
// Tests for the compiled Program and the (multi-threaded) buffer filter.
//

#include "line_filter.h"
#include "parser.h"
#include "program.h"
#include "shunting_yard.h"

#include <gtest/gtest.h>

#include <random>
#include <stdexcept>
#include <string>


namespace
{
  auto postfix(std::string const& expression) -> std::vector<alf::types::TokenBase>
  {
    auto tokens = alf::parser::parse_algebraic(expression);
    tokens = alf::parser::fill_in_missing_AND_symbols(tokens);
    return alf::shunting_yard(std::move(tokens));
  }

  auto run(std::string_view data, alf::Program const& p, unsigned threads) -> std::string
  {
    std::string out;
    alf::filter(data, p, threads, [&](std::string_view s) { out.append(s); });
    return out;
  }

  // The reference: split on '\n', strip "\r", ask passes_filters, keep the original bytes.
  auto naive(std::string_view data, std::vector<alf::types::TokenBase> const& tokens) -> std::string
  {
    std::string out;
    for (std::size_t p = 0; p < data.size();) {
      auto e = data.find('\n', p);
      auto next = e == std::string_view::npos ? data.size() : e + 1;
      auto text_end = e == std::string_view::npos ? data.size() : e;
      if (text_end > p and data[text_end - 1] == '\r') {
        --text_end;
      }
      if (alf::passes_filters(tokens, std::string(data.substr(p, text_end - p)))) {
        out.append(data.substr(p, next - p));
      }
      p = next;
    }
    return out;
  }

  auto random_expression(std::mt19937& rng, int depth) -> std::string
  {
    if (depth == 0 or rng() % 3 == 0) {
      std::string s = std::string("'") + static_cast<char>('a' + rng() % 4) + "'";
      if (rng() % 5 == 0) {
        s = "r'" + std::string(1, static_cast<char>('a' + rng() % 4)) + "[a-d]'";
      }
      return (rng() % 4 == 0 ? "!" : "") + s;
    }
    static char const* ops[] = { " & ", " | ", " ^ ", " " };
    std::string e = "(" + random_expression(rng, depth - 1) + ops[rng() % 4] + random_expression(rng, depth - 1) + ")";
    return rng() % 5 == 0 ? "!" + e : e;
  }

  auto random_text(std::mt19937& rng, std::size_t lines) -> std::string
  {
    std::string text;
    for (std::size_t i = 0; i < lines; ++i) {
      std::size_t len = rng() % 12;
      for (std::size_t k = 0; k < len; ++k) {
        text += static_cast<char>('a' + rng() % 5);
      }
      text += (rng() % 7 == 0) ? "\r\n" : "\n";
    }
    return text;
  }


  TEST(Program, agrees_with_passes_filters_on_random_expressions)
  {
    std::mt19937 rng(424242);
    std::string const text = random_text(rng, 200);
    for (int i = 0; i < 1500; ++i) {
      std::string e = random_expression(rng, 4);
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      ASSERT_EQ(run(text, p, 1), naive(text, tokens)) << e;
    }
  }

  TEST(Program, every_thread_count_gives_the_same_output)
  {
    std::mt19937 rng(7);
    std::string text = random_text(rng, 400000);  // a few MB, so the work really is split
    text += "abc";                                 // and a last line without '\n'
    auto tokens = postfix("('a' & !'b') | ('c' ^ r'd{2}')");
    alf::Program const p{ tokens };
    std::string const want = naive(text, tokens);
    for (unsigned threads : { 1u, 2u, 3u, 4u, 7u, 8u, 16u }) {
      EXPECT_EQ(run(text, p, threads), want) << threads << " threads";
    }
  }

  TEST(Program, small_inputs_and_edge_lines)
  {
    alf::Program const p{ postfix("'x'") };
    EXPECT_EQ(run("", p, 8), "");
    EXPECT_EQ(run("x", p, 8), "x");
    EXPECT_EQ(run("\n\nx\n\n", p, 8), "x\n");
    EXPECT_EQ(run("x\r\ny\r\nx", p, 8), "x\r\nx");
    alf::Program const anchored{ postfix("r'x$'") };
    EXPECT_EQ(run("ax\r\nxa\nbx", anchored, 1), "ax\r\nbx");  // the line ending is not part of the line
  }

  TEST(Program, empty_program_keeps_everything)
  {
    alf::Program const p{ {} };
    EXPECT_EQ(run("a\nb\n", p, 4), "a\nb\n");
  }

  TEST(Program, malformed_postfix_is_rejected)
  {
    using namespace alf::types;
    EXPECT_THROW(alf::Program({ SubStr{ std::string("a"), true }, operators::AND{} }), std::runtime_error);
    EXPECT_THROW(alf::Program({ SubStr{ std::string("a"), true }, SubStr{ std::string("b"), true } }), std::runtime_error);
    EXPECT_THROW(alf::Program({ operators::NOT{} }), std::runtime_error);
  }
}
