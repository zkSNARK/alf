//
// Tests for the compiled Program and the (multi-threaded) buffer filter.
//

#include "line_filter.h"
#include "parser.h"
#include "program.h"
#include "shunting_yard.h"

#include <gtest/gtest.h>

#include "../libalf/src/simd/simd.h"

#include <cstdlib>

#include <random>
#include <stdexcept>
#include <string>


namespace
{
  // ctest runs this suite once per SIMD backend with ALF_SIMD=<name> (tests/CMakeLists.txt).
  // If this machine can't run that backend, alf falls back to another one; skip rather than
  // test the wrong thing.
  class SimdEnvironment : public ::testing::Environment
  {
  public:
    void SetUp() override
    {
      if (char const* want = std::getenv("ALF_SIMD"); want and std::string_view(want) != alf::simd::active().name) {
        GTEST_SKIP() << "SIMD backend " << want << " doesn't run on this machine";
      }
      std::cout << "  SIMD backend: " << alf::simd::active().name << "\n";
    }
  };
  [[maybe_unused]] auto* const simd_environment = ::testing::AddGlobalTestEnvironment(new SimdEnvironment);

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

  // Text over a tiny alphabet so multi-character strings overlap and repeat a lot.
  auto ab_text(std::mt19937& rng, std::size_t lines) -> std::string
  {
    std::string text;
    for (std::size_t i = 0; i < lines; ++i) {
      std::size_t len = rng() % 30;
      for (std::size_t k = 0; k < len; ++k) {
        text += "abx"[rng() % 3];
      }
      text += (rng() % 9 == 0) ? "\r\n" : "\n";
    }
    return text;
  }

  TEST(Program, multi_string_search_with_overlapping_strings)
  {
    std::mt19937 rng(17);
    std::vector<std::string> words{ "ab", "ba", "aba", "bab", "xx", "abab", "x", "bxb" };
    for (int round = 0; round < 300; ++round) {
      std::string e;
      int terms = 1 + static_cast<int>(rng() % 6);
      for (int t = 0; t < terms; ++t) {
        if (t) {
          e += (rng() % 3 == 0) ? " | " : " & ";
        }
        e += (rng() % 2 ? "!'" : "'") + words[rng() % words.size()] + "'";
      }
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      std::string text = ab_text(rng, 200 + rng() % 200);
      if (rng() % 2) {
        text = "abab" + text + "bab";  // hits in the first and last bytes, outside any 64-byte block
      }
      std::string const want = naive(text, tokens);
      for (unsigned threads : { 1u, 3u }) {
        for (double fraction : { 2.0, -1.0, 0.25 }) {
          ASSERT_EQ(run(text, p, threads, fraction), want) << e << " threads " << threads << " fraction " << fraction;
        }
      }
    }
  }

  TEST(Program, many_strings_and_strings_with_line_breaks)
  {
    std::mt19937 rng(23);
    std::string const text = ab_text(rng, 2000);
    std::string many;  // 70 excluded strings: more than the 64 a bitset can hold
    for (int i = 0; i < 70; ++i) {
      many += "!'" + std::string(1, "ab"[i % 2]) + std::string(static_cast<std::size_t>(i % 5 + 1), 'x') + std::to_string(i) + "' ";
    }
    for (std::string e : { many, std::string("!'b\nab' & !'x'"), std::string("'a\r' | !'bb'"), std::string("'ab' & !'b\na'") }) {
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      std::string const want = naive(text, tokens);
      for (double fraction : { 2.0, -1.0, 0.25 }) {
        ASSERT_EQ(run(text, p, 2, fraction), want) << e.substr(0, 40) << " fraction " << fraction;
      }
    }
  }

  TEST(Program, settled)
  {
    // bit i = literals()[i] found so far; literals() are numbered in order of appearance
    alf::Program const excl{ postfix("!'a' & !'b'") };
    EXPECT_FALSE(excl.settled("", 0).has_value());  // nothing found yet: 'a' or 'b' could still appear
    EXPECT_EQ(excl.settled("", 0b01), false);        // found 'a': dropped whatever follows
    alf::Program const any{ postfix("'a' | 'b'") };
    EXPECT_EQ(any.settled("", 0b10), true);          // found 'b': kept
    EXPECT_FALSE(any.settled("", 0).has_value());
    alf::Program const both{ postfix("'a' & 'b'") };
    EXPECT_FALSE(both.settled("", 0b01).has_value()); // 'b' still needed
    EXPECT_EQ(both.settled("", 0b11), true);
    alf::Program const x{ postfix("'a' ^ 'b'") };
    EXPECT_FALSE(x.settled("", 0b01).has_value());   // 'b' turning up later would flip it
  }

  TEST(Program, settled_answers_hold_whatever_else_the_line_contains)
  {
    // If settled() gives an answer for the strings found so far, every line that contains them
    // (and any of the other strings) must get that answer.
    std::mt19937 rng(41);
    for (int round = 0; round < 2000; ++round) {
      std::string const e = random_expression(rng, 3);
      if (e.find("r'") != std::string::npos) {
        continue;
      }
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      auto const& lits = p.literals();
      std::uint64_t const all = (std::uint64_t{ 1 } << lits.size()) - 1;
      for (std::uint64_t found = 0; found <= all; ++found) {
        std::optional<bool> const answer = p.settled("", found);
        if (!answer) {
          continue;
        }
        for (std::uint64_t present = found; present <= all; present = (present + 1) | found) {
          std::string line;
          for (std::size_t i = 0; i < lits.size(); ++i) {
            if ((present >> i) & 1u) {
              line += lits[i] + " ";
            }
          }
          ASSERT_EQ(alf::passes_filters(tokens, line), *answer) << e << " found " << found << " line '" << line << "'";
          if (present == all) {
            break;
          }
        }
      }
    }
  }

  TEST(Program, many_strings_on_log_like_text)
  {
    // Teddy with frequent and absent strings, strings sharing a fingerprint, and short strings,
    // on text made of the same words: every grouping the scanner might pick must give the same
    // lines as the reference.
    std::vector<std::string> const words{ "code=200", "code=304", "code=404", "code=503", "user=7", "user=12",
                                          "seq=9", "rows=1", "rows=99", "job ", "POST", "[auth]", "[db]",
                                          "latency=3", "heartbeat", "DEBUG", "zz-absent-1", "zz-absent-2",
                                          "zz-absent-3", "ab", "x" };
    std::vector<std::string> const filler{ "INFO ", "code=2", "user=", "rows=", "seq=1", "latency=", "zz-", "POS",
                                           "[au", "job", "heart", " ", "2026-10-24T10:00:00Z ", "=", "1", "9" };
    std::mt19937 rng(77);
    std::string text;
    for (int line = 0; line < 3000; ++line) {
      int const n = static_cast<int>(rng() % 8);
      for (int k = 0; k < n; ++k) {
        text += (rng() % 4 == 0) ? words[rng() % (words.size() - 3)] : filler[rng() % filler.size()];
      }
      text += "\n";
    }
    for (int round = 0; round < 60; ++round) {
      std::string e;
      std::size_t const terms = 2 + rng() % words.size();
      for (std::size_t t = 0; t < terms; ++t) {
        e += std::string(rng() % 4 ? "!'" : "'") + words[rng() % words.size()] + "' ";
        if (rng() % 5 == 0 and t + 1 < terms) {
          e += "| ";
        }
      }
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      std::string const want = naive(text, tokens);
      for (unsigned threads : { 1u, 3u }) {
        ASSERT_EQ(run(text, p, threads), want) << e << " threads " << threads;
      }
    }
  }

  TEST(Program, more_than_64_strings_use_the_search_path_and_agree)
  {
    // Past 64 strings the found set takes several words; every answer must still match the
    // reference.  Strings share prefixes (so Teddy groups fill up and fingerprints repeat), and
    // some lines contain strings with bit positions in different words.
    std::mt19937 rng(91);
    std::string text;
    for (int line = 0; line < 1500; ++line) {
      int const n = static_cast<int>(rng() % 6);
      for (int k = 0; k < n; ++k) {
        text += "k" + std::to_string(rng() % 400) + (rng() % 3 ? " " : "=");
      }
      text += (rng() % 11 == 0) ? "\r\n" : "\n";
    }
    for (int round = 0; round < 12; ++round) {
      // (!'kA' | 'kB') & (!'kC' | 'kD') & ...: no string is required (an empty line passes), so
      // lines are decided from the found set, and the answer depends on exactly which strings a
      // line has: a bit recorded for the wrong string shows.  (An exclude list wouldn't show it,
      // since any string found drops the line.)
      std::size_t const pairs = 40 + rng() % 60;  // 80 .. 200 strings
      std::string e;
      for (std::size_t t = 0; t < pairs; ++t) {
        e += (t ? " & " : "") + std::string("(!'k") + std::to_string(rng() % 400) + "' | 'k" + std::to_string(rng() % 400) + "')";
      }
      auto tokens = postfix(e);
      alf::Program const p{ tokens };
      ASSERT_GT(p.found_words(), 1u) << e.substr(0, 60);
      ASSERT_FALSE(p.required_literals([](std::string_view) { return 1.0; }).has_value());
      std::string const want = naive(text, tokens);
      for (unsigned threads : { 1u, 3u }) {
        ASSERT_EQ(run(text, p, threads), want) << "round " << round << " threads " << threads;
      }
    }
  }

  TEST(Program, settled_and_matches_found_with_several_words)
  {
    std::string e;
    for (int i = 0; i < 70; ++i) {
      e += "!'s" + std::to_string(i) + "' ";
    }
    alf::Program const p{ postfix(e) };
    ASSERT_EQ(p.found_words(), 2u);
    std::uint64_t none[2] = { 0, 0 }, last_one[2] = { 0, std::uint64_t{ 1 } << 5 };  // literal 69
    EXPECT_FALSE(p.settled("", none).has_value());
    EXPECT_EQ(p.settled("", last_one), false);
    EXPECT_TRUE(p.matches_found("", none));
    EXPECT_FALSE(p.matches_found("", last_one));
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

  TEST(Program, long_runs_are_views_into_the_input)
  {
    // zero copy where it pays: a filter that keeps long runs of lines hands out views into the
    // buffer it was given rather than copies
    std::mt19937 rng(5);
    std::string const text = random_text(rng, 300000);
    auto tokens = postfix("!'zz'");  // keeps every line: one long contiguous run
    alf::Program const p{ tokens };
    for (double fraction : { 2.0, -1.0 }) {
      std::string out;
      std::size_t pieces = 0, inside = 0;
      alf::filter(text, p, 1, { [&](std::string_view v) {
        out.append(v);
        ++pieces;
        inside += (v.data() >= text.data() and v.data() + v.size() <= text.data() + text.size());
      }, {} }, fraction);
      EXPECT_EQ(out, naive(text, tokens));
      EXPECT_GT(pieces, 0u);
      EXPECT_EQ(inside, pieces) << "fraction " << fraction;
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
