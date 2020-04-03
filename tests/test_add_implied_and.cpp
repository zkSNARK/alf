//
// Created by Christopher Goebel on 3/29/20.
//

#include "alf_types.h"
#include "parser.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <numeric>

//#define VERBOSE

namespace {
  using namespace alf::types;

  auto print_vec(std::vector<alf::types::TokenBase>& v) {
    std::cout << "[";
    for (auto e = v.begin(); e != v.end(); ++e) {
      alf::types::TokenBase value = *e;
      std::cout << (e->required ? '+' : '-') << "'" << e->value
                << (e != v.end() - 1 ? "', " : "'");
    }
    std::cout << "]\n";
  }

  TEST(TestAddImpliedAnd, basic_all_positive_no_plus) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), true }},
        { operators::AND{}},
        { SubStr{ std::string("up"), true }},
    };
    
    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, basic_all_positive_with_plus) {
    // std::string input = "+ hello + world + whats + up";
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), true }},
        { operators::AND{}},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, basic_all_positive_with_plus_no_space) {
    // std::string input = "+hello +world +whats +up";
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { SubStr{ std::string("whats"), true }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), true }},
        { operators::AND{}},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, one_negative_3_implied_positive) {
    // std::string input = "a b c -d";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { SubStr{ std::string("c"), true }},
        { SubStr{ std::string("d"), false }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { SubStr{ std::string("c"), true }},
        { operators::AND{}},
        { SubStr{ std::string("d"), false }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, one_negative_2_positive) {
    // std::string input = "+a +b -c -d";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { SubStr{ std::string("c"), false }},
        { SubStr{ std::string("d"), false }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { SubStr{ std::string("c"), false }},
        { operators::AND{}},
        { SubStr{ std::string("d"), false }},
    };
    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, basic_all_positive_with_plus_missing) {
    // std::string input = "hello + world + whats + up";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { SubStr{ std::string("c"), true }},
        { SubStr{ std::string("d"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { SubStr{ std::string("c"), true }},
        { operators::AND{}},
        { SubStr{ std::string("d"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, basic_all_positive_half_plus_missing) {
    // std::string input = "+ hello world + whats up";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { SubStr{ std::string("c"), true }},
        { SubStr{ std::string("d"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { SubStr{ std::string("c"), true }},
        { operators::AND{}},
        { SubStr{ std::string("d"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, basic_all_positive_half_plus_missing_explicit_and) {
    // std::string input = "+hello & world& +whats &up";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { SubStr{ std::string("c"), true }},
        { operators::AND{}},
        { SubStr{ std::string("d"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { SubStr{ std::string("c"), true }},
        { operators::AND{}},
        { SubStr{ std::string("d"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, basic_all_negative) {
    // std::string input = "- hello - world - whats - up";
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), false }},
        { SubStr{ std::string("world"), false }},
        { SubStr{ std::string("whats"), false }},
        { SubStr{ std::string("up"), false }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), false }},
        { operators::AND{}},
        { SubStr{ std::string("world"), false }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), false }},
        { operators::AND{}},
        { SubStr{ std::string("up"), false }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }


  TEST(TestAddImpliedAnd, basic_all_negative_with_plus_missing) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), false }},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), false }},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), false }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { operators::AND{}},
        { SubStr{ std::string("whats"), false }},
        { operators::AND{}},
        { SubStr{ std::string("up"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }


  TEST(TestAddImpliedAnd, basic_parenthesis) {
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, no_space_parenthesis) {
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, no_space_right_parenthesis) {
    // (hello)(world)
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { operators::AND{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, weird_space_parenthesis) {
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { operators::AND{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, all_space_parenthesis) {
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("hello"), true }},
        { brackets::CloseParen{}},
        { operators::AND{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("world"), true }},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, and_symbol) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, or_symbol) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { operators::OR{}},
        { SubStr{ std::string("world"), true }}
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::OR{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, xor_symbol) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { operators::XOR{}},
        { SubStr{ std::string("world"), true }}
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::XOR{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, and_symbol_no_space_left) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, and_symbol_no_space_right) {
    std::vector<TokenBase> input{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("hello"), true }},
        { operators::AND{}},
        { SubStr{ std::string("world"), true }}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, empty_parenthesis) {
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { brackets::CloseParen{}}
    };
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, empty_parenthesis_w_spaces) {
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { brackets::CloseParen{}}
    };
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { brackets::CloseParen{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, lots_of_parenthesis_no_spaces) {
    // std::string input = "((((()))))";
    std::vector<TokenBase> input{
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

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, empty_squiggle_bracket_w_spaces) {
    // std::string input = "{ }";
    std::vector<TokenBase> input{
        { brackets::OpenCurlyBracket{}},
        { brackets::CloseCurlyBracket{}}
    };
    std::vector<TokenBase> expected{
        { brackets::OpenCurlyBracket{}},
        { brackets::CloseCurlyBracket{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, lots_of_squiggle_bracket_no_spaces) {
    // std::string input = "{{{{{}}}}}";
    std::vector<TokenBase> input{
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

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, empty_square_bracket_w_spaces) {
    // std::string input = "[ ]";
    std::vector<TokenBase> input{
        { brackets::OpenSquareBracket{}},
        { brackets::CloseSquareBracket{}}
    };
    std::vector<TokenBase> expected{
        { brackets::OpenSquareBracket{}},
        { brackets::CloseSquareBracket{}}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, lots_of_square_bracket_no_spaces) {
    // std::string input = "[[[[[]]]]]";
    std::vector<TokenBase> input{
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

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, quotes_in_str_no_spaces) {
    // std::string input = "a))b";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}},
        { SubStr{ std::string("b"), true }},
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, quotes_in_str_no_spaces2) {
    // std::string input = "a()b";
    std::vector<TokenBase> input{
        { SubStr{ std::string("a"), true }},
        { brackets::OpenParen{}},
        { brackets::CloseParen{}},
        { SubStr{ std::string("b"), true }},
    };
    std::vector<TokenBase> expected{
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { brackets::OpenParen{}},
        { brackets::CloseParen{}},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, complex1) {
    // std::string input = "( hello ( world | meet universe ) )";
    std::vector<TokenBase> input{
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

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, complex1_no_space) {
    // std::string input = "(hello(world|meet universe))";
    std::vector<TokenBase> input{
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

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, complex1_all_operators_no_space) {
    // std::string input = "(hello&(world|meet&universe))";
    std::vector<TokenBase> input{
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

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, some_quotes_and_paren) {
    // std::string input = "(a b (c'd d  d'))";
    std::vector<TokenBase> input{
        { brackets::OpenParen{}},
        { SubStr{ std::string("a"), true }},
        { SubStr{ std::string("b"), true }},
        { brackets::OpenParen{}},
        { SubStr{ std::string("c"), true }},
        { SubStr{ std::string("d d  d"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };
    std::vector<TokenBase> expected{
        { brackets::OpenParen{}},
        { SubStr{ std::string("a"), true }},
        { operators::AND{}},
        { SubStr{ std::string("b"), true }},
        { operators::AND{}},
        { brackets::OpenParen{}},
        { SubStr{ std::string("c"), true }},
        { operators::AND{}},
        { SubStr{ std::string("d d  d"), true }},
        { brackets::CloseParen{}},
        { brackets::CloseParen{}}
    };
    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestAddImpliedAnd, some_quotes_and_negations) {
    // std::string input = "'abc d -'- a -'-e' - f g";
    std::vector<TokenBase> input{
        { SubStr{ std::string("abc d -"), true }},
        { SubStr{ std::string("a"), false }},
        { SubStr{ std::string("-e"), false }},
        { SubStr{ std::string("f"), false }},
        { SubStr{ std::string("g"), true }}
    };

    std::vector<TokenBase> expected{
        { SubStr{ std::string("abc d -"), true }},
        { operators::AND{}},
        { SubStr{ std::string("a"), false }},
        { operators::AND{}},
        { SubStr{ std::string("-e"), false }},
        { operators::AND{}},
        { SubStr{ std::string("f"), false }},
        { operators::AND{}},
        { SubStr{ std::string("g"), true }}
    };

    std::vector<alf::types::TokenBase> result{
        alf::fill_in_missing_AND_symbols(input)
    };

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
#endif
    ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }
}  // anon namespace//
