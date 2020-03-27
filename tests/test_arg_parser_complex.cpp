//
// Created by Christopher Goebel on 3/24/20.
//

#include "libalf.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <numeric>

auto print_vec(std::vector<std::string> const &v) {
  for (auto i: v) {
    std::cout << i << ' ';
  }
  std::cout << '\n';
}
auto print_vec2(std::vector<std::string> const &v) {
  std::cout << "{";
  for (auto x = v.begin(); x != v.end(); ++x) {
    std::cout << "\"" << *x << (x != v.end() - 1? "\", " : "\"");
  }
  std::cout << "}";
  std::cout << '\n';
}

auto replace(std::string s, std::string target, std::string replacement) {
  size_t pos = s.find(target);

  while (pos != std::string::npos) {
    s.replace(pos, target.size(), replacement);
    pos = s.find(target, pos + replacement.size());
  }
  return std::move(s);
}

auto join_vec(std::vector<std::string> const &v) {
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

  TEST(TestVecCompare, basic_all_positive_no) {
    std::string input = "hello world whats up";
    std::vector<std::string> expected{"hello", "&", "world", "&", "whats", "&", "up"};
    
    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, quoted_string) {
    std::string input = "'world whats'";
    std::vector<std::string> expected = {"world whats"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, basic_parenthesis) {
    std::string input = "( hello world )";
    std::vector<std::string> expected{"(", "hello", "&", "world", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, no_space_parenthesis) {

    std::string input = "(hello world)";
    std::vector<std::string> expected{"(", "hello", "&", "world", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, no_space_right_parenthesis) {
    std::string input = "(hello )(world)";
    std::vector<std::string> expected{"(", "hello", ")", "(", "world", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, weird_space_parenthesis) {
    std::string input = "(hello )( world)";
    std::vector<std::string> expected{"(", "hello", ")", "(", "world", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, all_space_parenthesis) {
    std::string input = "( hello ) ( world )";
    std::vector<std::string> expected{"(", "hello", ")", "(", "world", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, and_symbol) {
    std::string input = "hello & world";
    std::vector<std::string> expected{"hello", "&", "world"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, and_symbol_no_space_left) {
    std::string input = "hello& world";
    std::vector<std::string> expected{"hello", "&", "world"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, and_symbol_no_space_right) {
    std::string input = "hello &world";
    std::vector<std::string> expected{"hello", "&", "world"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, empty_parenthesis) {
    std::string input = "()";
    std::vector<std::string> expected{"(", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, empty_parenthesis_w_spaces) {
    std::string input = "( )";
    std::vector<std::string> expected{"(", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, lots_of_parenthesis_no_spaces) {
    std::string input = "((((()))))";
    std::vector<std::string> expected{"(", "(", "(", "(", "(", ")", ")", ")", ")", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, empty_squiggle_bracket_w_spaces) {
    std::string input = "{ }";
    std::vector<std::string> expected{"{", "}"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, lots_of_squiggle_bracket_no_spaces) {
    std::string input = "{{{{{}}}}}";
    std::vector<std::string> expected{"{", "{", "{", "{", "{", "}", "}", "}", "}", "}"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, empty_square_bracket_w_spaces) {
    std::string input = "[ ]";
    std::vector<std::string> expected{"[", "]"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, lots_of_square_bracket_no_spaces) {
    std::string input = "[[[[[]]]]]";
    std::vector<std::string> expected{"[", "[", "[", "[", "[", "]", "]", "]", "]", "]"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, complex1) {
    std::string input = "( hello ( world | meet universe ) )";
    std::vector<std::string> expected{"(", "hello", "&", "(", "world", "|", "meet", "&", "universe", ")", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, complex1_no_space) {
    std::string input = "(hello(world|meet universe))";
    std::vector<std::string> expected{"(", "hello", "&", "(", "world", "|", "meet", "&", "universe", ")", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }

  TEST(TestVecCompare, complex1_all_operators_no_space) {
    std::string input = "(hello&(world|meet&universe))";
    std::vector<std::string> expected{"(", "hello", "&", "(", "world", "|", "meet", "&", "universe", ")", ")"};

    std::vector<std::string> pack{parse_for_shunting(input)};
    ASSERT_THAT(pack, testing::ElementsAreArray(expected));
  }
}  // anon namespace