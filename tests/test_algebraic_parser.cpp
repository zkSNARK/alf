//
// Created by Christopher Goebel on 3/28/20.
//

#include "alf_types.h"
#include "libalf.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <numeric>

auto print_vec(std::vector<alf::types::TokenBase>& v) {
  std::cout << "[";
  for (auto e = v.begin(); e != v.end(); ++e) {
    alf::types::TokenBase value = *e;
    std::cout << (e->required ? '+' : '-') << "'" << e->value
              << (e != v.end() - 1 ? "', " : "'");
  }
  std::cout << "]\n";
}

auto print_vec2(std::vector<std::string> const& v) {
  std::cout << "{";
  for (auto x = v.begin(); x != v.end(); ++x) {
    std::cout << "\"" << *x << (x != v.end() - 1 ? "\", " : "\"");
  }
  std::cout << "}";
  std::cout << '\n';
}

auto replace(std::string s, std::string const& target, std::string const& replacement) {
  size_t pos = s.find(target);

  while (pos != std::string::npos) {
    s.replace(pos, target.size(), replacement);
    pos = s.find(target, pos + replacement.size());
  }
  return std::move(s);
}

auto join_vec(std::vector<std::string> const& v) {
  std::ostringstream vts;
  if (!v.empty()) {
    // Convert all but the last element to avoid a trailing ","
    std::copy(v.begin(), v.end() - 1, std::ostream_iterator<std::string>(vts, " "));

    // Now add the last element with no delimiter
    vts << v.back();
  }
  return vts.str();
}

namespace {

  using namespace alf::types;

  TEST(TestAlgParserBasic, basic_all_positive_no_plus) {
    std::string input = "hello world whats up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserUnaryOperators, basic_all_positive_with_plus) {
    std::string input = "+ hello + world + whats + up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserUnaryOperators, basic_all_positive_with_plus_no_space) {
    std::string input = "+hello +world +whats +up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserUnaryOperators, one_negative_3_implied_positive) {
    std::string input = "a b c -d";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { SubStr{ std::string("c"), true }},
        { SubStr{ std::string("d"), false }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserUnaryOperators, one_negative_2_positive) {
    std::string input = "+a +b -c -d";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { SubStr{ std::string("c"), false }},
        { SubStr{ std::string("d"), false }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserUnaryOperators, basic_all_positive_with_plus_missing) {
    std::string input = "hello + world + whats + up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBasic, basic_all_positive_half_plus_missing) {
    std::string input = "+ hello world + whats up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBasic, basic_all_positive_half_plus_missing_explicit_and) {
    std::string input = "+hello & world& +whats &up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), true }},
        { operators::AND{}},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBasic, basic_all_negative) {
    std::string input = "- hello - world - whats - up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), false }},
        { SubStr{ std::string("world"), false }},
        { SubStr{ std::string("whats"), false }},
        { SubStr{ std::string("up"), false }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }


  TEST(TestAlgParserBasic, basic_all_negative_with_plus_missing) {
    std::string input = "- hello world & - whats up";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), false }},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), false }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };

    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserQuotes, quoted_string) {
    std::string input = "'world whats'";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("world whats"), true }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserQuotes, quoted_string_neg) {
    std::string input = "-'world whats'";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("world whats"), false }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, basic_parenthesis) {
    std::string input = "( hello world )";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, no_space_parenthesis) {
    std::string input = "(hello world)";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, no_space_right_parenthesis) {
    std::string input = "(hello )(world)";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, weird_space_parenthesis) {
    std::string input = "(hello )( world)";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, all_space_parenthesis) {
    std::string input = "( hello ) ( world )";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserOperators, and_symbol) {
    std::string input = "hello & world";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserOperators, or_symbol) {
    std::string input = "hello | world";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::OR{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserOperators, xor_symbol) {
    std::string input = "hello ^ world";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::XOR{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserOperators, and_symbol_no_space_left) {
    std::string input = "hello& world";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserOperators, and_symbol_no_space_right) {
    std::string input = "hello &world";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, empty_parenthesis) {
    std::string input = "()";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, empty_parenthesis_w_spaces) {
    std::string input = "( )";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, lots_of_parenthesis_no_spaces) {
    std::string input = "((((()))))";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { brackets::OpenParen{}},
        { brackets::OpenParen{}},
        { brackets::OpenParen{}},
        { brackets::OpenParen{}},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, empty_squiggle_bracket_w_spaces) {
    std::string input = "{ }";
    std::vector<TokenBase> expected{
        { brackets::OpenCurlyBracket{}},
        { brackets::CloseCurlyBracket{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, lots_of_squiggle_bracket_no_spaces) {
    std::string input = "{{{{{}}}}}";
    std::vector<TokenBase> expected{
        { brackets::OpenCurlyBracket{}},
        { brackets::OpenCurlyBracket{}},
        { brackets::OpenCurlyBracket{}},
        { brackets::OpenCurlyBracket{}},
        { brackets::OpenCurlyBracket{}},
        { brackets::CloseCurlyBracket{}},
        { brackets::CloseCurlyBracket{}},
        { brackets::CloseCurlyBracket{}},
        { brackets::CloseCurlyBracket{}},
        { brackets::CloseCurlyBracket{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, empty_square_bracket_w_spaces) {
    std::string input = "[ ]";
    std::vector<TokenBase> expected{
        { brackets::OpenSquareBracket{}},
        { brackets::CloseSquareBracket{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, lots_of_square_bracket_no_spaces) {
    std::string input = "[[[[[]]]]]";
    std::vector<TokenBase> expected{
        { brackets::OpenSquareBracket{}},
        { brackets::OpenSquareBracket{}},
        { brackets::OpenSquareBracket{}},
        { brackets::OpenSquareBracket{}},
        { brackets::OpenSquareBracket{}},
        { brackets::CloseSquareBracket{}},
        { brackets::CloseSquareBracket{}},
        { brackets::CloseSquareBracket{}},
        { brackets::CloseSquareBracket{}},
        { brackets::CloseSquareBracket{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, quotes_in_str_no_spaces) {
    std::string input = "a))b";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}},
        { SubStr{ std::string("b"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserBrackets, quotes_in_str_no_spaces2) {
    std::string input = "a()b";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { brackets::OpenParen{}},
        { brackets::CloseParen{}},
        { SubStr{ std::string("b"), true }},
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserComplex, complex1) {
    std::string input = "( hello ( world | meet universe ) )";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { operators::OR{}},
        { SubStr{ std::string("meet"), true }},
        { SubStr{ std::string("universe"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserComplex, complex1_no_space) {
    std::string input = "(hello(world|meet universe))";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { operators::OR{}},
        { SubStr{ std::string("meet"), true }},
        { SubStr{ std::string("universe"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserComplex, complex1_all_operators_no_space) {
    std::string input = "(hello&(world|meet&universe))";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { operators::OR{}},
        { SubStr{ std::string("meet"), true }},
        { operators::AND{}},
        { SubStr{ std::string("universe"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };
    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserComplex, some_quotes_and_paren) {
    std::string input = "(a b (c'd d  d'))";
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { brackets::OpenParen{}},
        { SubStr{ std::string("c"), true }},
        { SubStr{ std::string("d d  d"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };
    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestAlgParserComplex, some_quotes_and_negations) {
    std::string input = "'abc d -'- a -'-e' - f g";
    std::vector<TokenBase> expected{
        { SubStr{ std::string("abc d -"), true }},
        { SubStr{ std::string("a"), false }},
        { SubStr{ std::string("-e"), false }},
        { SubStr{ std::string("f"), false }},
        { SubStr{ std::string("g"), true }}
    };
    std::vector<alf::types::TokenBase> pack{ alf::parse_algebraic(input) };
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

}  // anon namespace//
