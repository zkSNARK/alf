//
// Created by Christopher Goebel on 3/24/20.
//

#include "alf_types.h"
#include "shunting_yard.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <numeric>

//#define VERBOSE

namespace {
  using namespace alf::types;

  auto print_vec(std::vector<alf::types::TokenBase> const& v) {
    std::cout << "[";
    for (auto e = v.begin(); e != v.end(); ++e) {
      alf::types::TokenBase value = *e;
      std::cout << (e->required ? '+' : '-') << "'" << e->value
                << (e != v.end() - 1 ? "', " : "'");
    }
    std::cout << "]\n";
  }

  TEST(TestShuntingYard, all_and_positive) {
    std::vector<TokenBase> input{
      { SubStr{ std::string("a"), true }},
      { operators::AND{}},
      { SubStr{ std::string("b"), true }},
      { operators::AND{}},
      { SubStr{ std::string("c"), true }},
      { operators::AND{}},
      { SubStr{ std::string("d"), true }},
    };
    std::vector<TokenBase> expected {
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("b"), true }},
      { SubStr{ std::string("c"), true }},
      { SubStr{ std::string("d"), true }},
      { operators::AND{}},
      { operators::AND{}},
      { operators::AND{}}
    };

    std::vector<alf::types::TokenBase> result {alf::shunting_yard(input)};


#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result);
#endif
     ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }

  TEST(TestShuntingYard, all_and_negative_makes_no_difference) {
    std::vector<TokenBase> input{
      { SubStr{ std::string("a"), false }},
      { operators::AND{}},
      { SubStr{ std::string("b"), false }},
      { operators::AND{}},
      { SubStr{ std::string("c"), false }},
      { operators::AND{}},
      { SubStr{ std::string("d"), false }},
    };

    std::vector<TokenBase> expected {
      { SubStr{ std::string("a"), false }},
      { SubStr{ std::string("b"), false }},
      { SubStr{ std::string("c"), false }},
      { SubStr{ std::string("d"), false }},
      { operators::AND{}},
      { operators::AND{}},
      { operators::AND{}}
    };

    std::vector<alf::types::TokenBase> result {alf::shunting_yard(input)};

#ifdef VERBOSE
    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result);
#endif
     ASSERT_THAT(result, testing::ElementsAreArray(expected));
  }
//
//  TEST(TestShuntingYard, one_negative_2_positive) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("a"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("c"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("d"), false }},
//    };
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, basic_all_positive_with_plus_missing) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("a"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("c"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("d"), true }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, basic_all_positive_half_plus_missing) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("a"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("c"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("d"), true }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, basic_all_positive_half_plus_missing_explicit_and) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("a"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("c"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("d"), true }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, basic_all_negative) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("whats"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("up"), false }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//
//  TEST(TestShuntingYard, basic_all_negative_with_plus_missing) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("whats"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("up"), true }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//
//  TEST(TestShuntingYard, basic_parenthesis) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), true }},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, no_space_parenthesis) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), true }},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, no_space_right_parenthesis) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { brackets::CloseParen{}},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("world"), true }},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, weird_space_parenthesis) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { brackets::CloseParen{}},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("world"), true }},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, all_space_parenthesis) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { brackets::CloseParen{}},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("world"), true }},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, and_symbol) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), true }}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, or_symbol) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), true }},
//      { operators::OR{}},
//      { SubStr{ std::string("world"), true }}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, xor_symbol) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), true }},
//      { operators::XOR{}},
//      { SubStr{ std::string("world"), true }}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, and_symbol_no_space_left) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), true }}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, and_symbol_no_space_right) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("world"), true }}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, empty_parenthesis) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, empty_parenthesis_w_spaces) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, lots_of_parenthesis_no_spaces) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { brackets::OpenParen{}},
//      { brackets::OpenParen{}},
//      { brackets::OpenParen{}},
//      { brackets::OpenParen{}},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, empty_squiggle_bracket_w_spaces) {
//    std::vector<TokenBase> input{
//      { brackets::OpenCurlyBracket{}},
//      { brackets::CloseCurlyBracket{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, lots_of_squiggle_bracket_no_spaces) {
//    std::vector<TokenBase> input{
//      { brackets::OpenCurlyBracket{}},
//      { brackets::OpenCurlyBracket{}},
//      { brackets::OpenCurlyBracket{}},
//      { brackets::OpenCurlyBracket{}},
//      { brackets::OpenCurlyBracket{}},
//      { brackets::CloseCurlyBracket{}},
//      { brackets::CloseCurlyBracket{}},
//      { brackets::CloseCurlyBracket{}},
//      { brackets::CloseCurlyBracket{}},
//      { brackets::CloseCurlyBracket{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, empty_square_bracket_w_spaces) {
//    std::vector<TokenBase> input{
//      { brackets::OpenSquareBracket{}},
//      { brackets::CloseSquareBracket{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, lots_of_square_bracket_no_spaces) {
//    std::vector<TokenBase> input{
//      { brackets::OpenSquareBracket{}},
//      { brackets::OpenSquareBracket{}},
//      { brackets::OpenSquareBracket{}},
//      { brackets::OpenSquareBracket{}},
//      { brackets::OpenSquareBracket{}},
//      { brackets::CloseSquareBracket{}},
//      { brackets::CloseSquareBracket{}},
//      { brackets::CloseSquareBracket{}},
//      { brackets::CloseSquareBracket{}},
//      { brackets::CloseSquareBracket{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, quotes_in_str_no_spaces) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("a"), true }},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, quotes_in_str_no_spaces2) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("a"), true }},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { brackets::CloseParen{}},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, complex1) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("world"), true }},
//      { operators::OR{}},
//      { SubStr{ std::string("meet"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("universe"), true }},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, complex1_no_space) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("world"), true }},
//      { operators::OR{}},
//      { SubStr{ std::string("meet"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("universe"), true }},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, complex1_all_operators_no_space) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("hello"), true }},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("world"), true }},
//      { operators::OR{}},
//      { SubStr{ std::string("meet"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("universe"), true }},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, some_quotes_and_paren) {
//    std::vector<TokenBase> input{
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("a"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("b"), true }},
//      { operators::AND{}},
//      { brackets::OpenParen{}},
//      { SubStr{ std::string("c"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("d d  d"), true }},
//      { brackets::CloseParen{}},
//      { brackets::CloseParen{}}
//    };
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
//
//  TEST(TestShuntingYard, some_quotes_and_negations) {
//    std::vector<TokenBase> input{
//      { SubStr{ std::string("abc d -"), true }},
//      { operators::AND{}},
//      { SubStr{ std::string("a"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("-e"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("f"), false }},
//      { operators::AND{}},
//      { SubStr{ std::string("g"), true }}
//    };
//
//    std::vector<alf::types::TokenBase> result{
//      alf::fill_in_missing_AND_symbols(input)
//    };
//
//#ifdef VERBOSE
//    std::cout << "expected  : ";
//    print_vec(expected);
//    std::cout << "actual    : ";
//    print_vec(result);
//#endif
//    // ASSERT_THAT(result, testing::ElementsAreArray(expected));
//  }
}  // anon namespace//
