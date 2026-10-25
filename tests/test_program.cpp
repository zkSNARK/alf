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

  // max_hit_fraction 2.0 forces the string-search path wherever it applies, -1.0 forces reading
  // every line, and the default lets filter() choose.
  auto run(std::string_view data, alf::Program const& p, unsigned threads, double max_hit_fraction = 0.25) -> std::string
  {
    std::string out;
    alf::filter(data, p, threads, { [&](std::string_view s) { out.append(s); }, {} }, max_hit_fraction);
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

  // Mostly filler, with the letters the expressions look for appearing now and then, so that
  // few lines contain a string and filter() takes the search path rather than reading every line.
  auto sparse_text(std::mt19937& rng, std::size_t lines) -> std::string
  {
    std::string text;
    for (std::size_t i = 0; i < lines; ++i) {
      std::size_t len = rng() % 40;
      for (std::size_t k = 0; k < len; ++k) {
        text += (rng() % 60 == 0) ? static_cast<char>('a' + rng() % 4) : static_cast<char>('w' + rng() % 4);
      }
      text += (rng() % 7 == 0) ? "\r\n" : "\n";
    }
    return text;
  }

  TEST(Program, string_search_path_agrees_with_line_by_line)
  {
    // regex-free expressions take the search path; check it on sparse and dense inputs, with
    // strings at the very start and end of the data, CRLF, empty lines and no final newline
    std::mt19937 rng(99);
    for (int round = 0; round < 400; ++round) {
      std::string e = random_expression(rng, 3);
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      std::string text = (rng() % 4 ? sparse_text(rng, 300 + rng() % 300) : random_text(rng, 300 + rng() % 300));
      if (rng() % 3 == 0) {
        text = "ab" + text + "cd";  // hits at the very first and very last bytes
      }
      if (rng() % 2) {
        text.pop_back();  // no final newline
      }
      std::string const want = naive(text, tokens);
      for (unsigned threads : { 1u, 4u }) {
        for (double fraction : { 2.0, -1.0, 0.25 }) {
          ASSERT_EQ(run(text, p, threads, fraction), want) << e << " threads " << threads << " fraction " << fraction;
        }
      }
    }
  }

  TEST(Program, required_literals)
  {
    auto const len = [](std::string_view s) { return static_cast<double>(s.size()); };
    using V = std::vector<std::string>;
    EXPECT_EQ(alf::Program{ postfix("'aa' & 'b' & 'ccc'") }.required_literals(len), V{ "b" });  // cheapest side of &
    EXPECT_EQ(alf::Program{ postfix("'a' | 'b'") }.required_literals(len), (V{ "a", "b" }));    // both sides of |
    EXPECT_EQ(alf::Program{ postfix("('a' & !'x') | ('b' & 'yy')") }.required_literals(len), (V{ "a", "b" }));
    EXPECT_EQ(alf::Program{ postfix("r'code=5\\d\\d' & r'latency=\\d'") }.required_literals(len), V{ "code=5" });
    EXPECT_FALSE(alf::Program{ postfix("!'a'") }.required_literals(len).has_value());
    EXPECT_FALSE(alf::Program{ postfix("'a' | !'b'") }.required_literals(len).has_value());
    EXPECT_FALSE(alf::Program{ postfix("!('a' & 'b')") }.required_literals(len).has_value());
    EXPECT_FALSE(alf::Program{ postfix("r'x|y'") }.required_literals(len).has_value());
  }

  TEST(Program, regex_required_prefix)
  {
    EXPECT_EQ(alf::regex_required_prefix("code=5\\d\\d"), "code=5");
    EXPECT_EQ(alf::regex_required_prefix("^abc"), "abc");
    EXPECT_EQ(alf::regex_required_prefix("ab*c"), "a");
    EXPECT_EQ(alf::regex_required_prefix("ab?"), "a");
    EXPECT_EQ(alf::regex_required_prefix("ab{2}"), "a");
    EXPECT_EQ(alf::regex_required_prefix("ab+"), "ab");
    EXPECT_EQ(alf::regex_required_prefix("a|b"), "");
    EXPECT_EQ(alf::regex_required_prefix("(ab)"), "");
    EXPECT_EQ(alf::regex_required_prefix(".x"), "");
    EXPECT_EQ(alf::regex_required_prefix("[ab]c"), "");
  }

  TEST(Program, answer_without_literals)
  {
    EXPECT_EQ(alf::Program{ postfix("'a' & !'b'") }.answer_without_literals(), false);
    EXPECT_EQ(alf::Program{ postfix("!'a' !'b'") }.answer_without_literals(), true);
    EXPECT_EQ(alf::Program{ postfix("'a' ^ !'b'") }.answer_without_literals(), true);
    EXPECT_FALSE(alf::Program{ postfix("'a' | r'b'") }.answer_without_literals().has_value());
    EXPECT_FALSE(alf::Program{ postfix("'a' | ''") }.answer_without_literals().has_value());
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

  TEST(Program, output_is_views_into_the_input)
  {
    // zero copy on one thread: every piece filter() hands out points into the buffer it was given
    std::mt19937 rng(5);
    std::string const text = random_text(rng, 300000) + sparse_text(rng, 300000);
    auto tokens = postfix("('a' & !'b') | 'c'");
    alf::Program const p{ tokens };
    for (unsigned threads : { 1u }) {
      for (double fraction : { 2.0, -1.0 }) {
        std::string out;
        std::size_t pieces = 0;
        alf::filter(text, p, threads, { [&](std::string_view v) {
          ASSERT_GE(v.data(), text.data());
          ASSERT_LE(v.data() + v.size(), text.data() + text.size());
          out.append(v);
          ++pieces;
        }, {} }, fraction);
        EXPECT_EQ(out, naive(text, tokens));
        EXPECT_GT(pieces, 0u);
      }
    }
  }

  TEST(Program, views_held_until_flush_stay_valid)
  {
    // Like alf's writer: keep the views and only read them in flush() (or, for views into the
    // input, after filter() returns).  A view into a buffer filter() has already freed would be
    // a use-after-free here, which AddressSanitizer reports.
    std::mt19937 rng(11);
    std::string const text = random_text(rng, 400000);  // dense matches: threads pack their output
    auto tokens = postfix("'a' & !'b'");
    alf::Program const p{ tokens };
    for (unsigned threads : { 1u, 2u, 8u }) {
      std::vector<std::string_view> held;
      std::string out;
      auto const drain = [&] {
        for (auto v : held) {
          out.append(v);
        }
        held.clear();
      };
      alf::filter(text, p, threads, { [&](std::string_view v) { held.push_back(v); }, drain });
      drain();  // what's left points into `text`, which is still alive
      EXPECT_EQ(out, naive(text, tokens)) << threads << " threads";
    }
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
