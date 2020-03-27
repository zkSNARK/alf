//
// Created by Christopher Goebel on 3/24/20.
//

#include "libalf.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>
#include <numeric>

auto print_vec(std::vector<std::string_view> const &v) {
  for (auto i: v) {
    std::cout << i << ' ';
  }
  std::cout << '\n';
}

auto replace(std::string s, std::string_view target, std::string_view replacement) {
  size_t pos = s.find(target);

  while (pos != std::string::npos) {
    s.replace(pos, target.size(), replacement);
    pos = s.find(target, pos + replacement.size());
  }
  return std::move(s);
}

auto join_vec(std::vector<std::string_view> const &v) {
  std::ostringstream vts;
  if (!v.empty()) {
    // Convert all but the last element to avoid a trailing ","
    std::copy(v.begin(), v.end() - 1, std::ostream_iterator<std::string_view>(vts, " "));

    // Now add the last element with no delimiter
    vts << v.back();
  }
  return vts.str();
}

namespace {
  template<typename T, int size>
  int array_size(T(&)[size]) {
    return size;
  }


  TEST(TestVecCompare, basic_all_positive_no) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector<std::string_view> e = {e1, e2, e3, e4};

    std::string input = e1 + " " + e2 + " " + e3 + " " + e4;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, quoted_string) {
    std::string_view i1 = "'world whats'";
    std::vector<std::string_view> pack{parse_for_shunting(i1)};

    std::string e1 = replace(std::string{i1}, "'", "");
    std::vector<std::string_view> e = {e1};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, basic_parenthesis) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = "world";
    std::string i4 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4};

    // "( hello world )"
    std::string input = i1 + " " + i2 + " " + i3 + " " + i4;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, no_space_parenthesis) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = "world";
    std::string i4 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4};

    // "(hello world)"
    std::string input = i1 + i2 + " " + i3 + i4;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, no_space_right_parenthesis) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = ")";
    std::string i4 = "(";
    std::string i5 = "world";
    std::string i6 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6};

    // "(hello )(world)"
    std::string input = i1 + i2 + " " + i3 + i4 + i5 + i6;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, weird_space_parenthesis) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = ")";
    std::string i4 = "(";
    std::string i5 = "world";
    std::string i6 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6};

    // "(hello )( world)"
    std::string input = i1 + i2 + " " + i3 + i4 + " " + i5 + i6;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, all_space_parenthesis) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = ")";
    std::string i4 = "(";
    std::string i5 = "world";
    std::string i6 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6};

    // "( hello ) ( world )"
    std::string input = i1 + " " + i2 + " " + i3 + " " + i4 + " " + i5 + " " + i6;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, and_symbol) {
    std::string i1 = "hello";
    std::string i2 = "&";
    std::string i3 = "world";
    std::vector<std::string_view> e = {i1, i2, i3};

    // "hello & world"
    std::string input = i1 + " " + i2 + " " + i3;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, and_symbol_no_space_left) {
    std::string i1 = "hello";
    std::string i2 = "&";
    std::string i3 = "world";
    std::vector<std::string_view> e = {i1, i2, i3};

    // "hello& world"
    std::string input = i1 + i2 + " " + i3;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, and_symbol_no_space_right) {
    std::string i1 = "hello";
    std::string i2 = "&";
    std::string i3 = "world";
    std::vector<std::string_view> e = {i1, i2, i3};

    // "hello &world"
    std::string input = i1 + " " + i2 + i3;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, empty_parenthesis) {
    std::string i1 = "(";
    std::string i2 = ")";
    std::vector<std::string_view> e = {i1, i2};

    // "()"
    std::string input = i1 + i2;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, empty_parenthesis_w_spaces) {
    std::string i1 = "(";
    std::string i2 = ")";
    std::vector<std::string_view> e = {i1, i2};

    // "( )"
    std::string input = i1 + " " + i2;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, lots_of_parenthesis_no_spaces) {
    std::string i1 = "(";
    std::string i2 = "(";
    std::string i3 = "(";
    std::string i4 = "(";
    std::string i5 = "(";
    std::string i6 = ")";
    std::string i7 = ")";
    std::string i8 = ")";
    std::string i9 = ")";
    std::string i10 = ")";

    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9, i10};

    // "((((()))))"
    std::string input = i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, empty_squiggle_bracket_w_spaces) {
    std::string i1 = "{";
    std::string i2 = "}";
    std::vector<std::string_view> e = {i1, i2};

    // "{ }"
    std::string input = i1 + " " + i2;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, lots_of_squiggle_bracket_no_spaces) {
    std::string i1 = "{";
    std::string i2 = "{";
    std::string i3 = "{";
    std::string i4 = "{";
    std::string i5 = "{";
    std::string i6 = "}";
    std::string i7 = "}";
    std::string i8 = "}";
    std::string i9 = "}";
    std::string i10 = "}";

    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9, i10};

    // "{{{{{}}}}}"
    std::string input = i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    std::string input2 = join_vec(e);
    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, empty_square_bracket_w_spaces) {
    std::string i1 = "{";
    std::string i2 = "}";
    std::vector<std::string_view> e = {i1, i2};

    // "{ }"
    std::string input = i1 + " " + i2;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    std::string input2 = join_vec(e);
    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, lots_of_square_bracket_no_spaces) {
    std::string i1 = "[";
    std::string i2 = "[";
    std::string i3 = "[";
    std::string i4 = "[";
    std::string i5 = "[";
    std::string i6 = "]";
    std::string i7 = "]";
    std::string i8 = "]";
    std::string i9 = "]";
    std::string i10 = "]";

    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9, i10};

    // "[[[[[]]]]]"
    std::string input = i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, complex1) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = "(";
    std::string i4 = "world";
    std::string i5 = "|";
    std::string i6 = "meet";
    std::string i7 = "universe";
    std::string i8 = ")";
    std::string i9 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9};

    // "( hello ( world | meet universe ) )"
    std::string input = i1 + " " + i2 + " " + i3 + " " + i4 + " " + i5 + " " + i6 + " " + i7 + " " + i8 + " " + i9;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    std::cout << "expected ";
    print_vec(e);
    std::cout << "got ";
    print_vec(pack);

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, complex1_no_space) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = "(";
    std::string i4 = "world";
    std::string i5 = "|";
    std::string i6 = "meet";
    std::string i7 = "universe";
    std::string i8 = ")";
    std::string i9 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9};

    // "(hello(world|meet universe))"
    std::string input = i1 + i2 + i3 + i4 + i5 + i6 + " " + i7 + i8 + i9;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    std::cout << "expected ";
    print_vec(e);
    std::cout << "got ";
    print_vec(pack);

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

  TEST(TestVecCompare, complex1_all_operators_no_space) {
    std::string i1 = "(";
    std::string i2 = "hello";
    std::string i3 = "&";
    std::string i4 = "(";
    std::string i5 = "world";
    std::string i6 = "|";
    std::string i7 = "meet";
    std::string i8 = "&";
    std::string i9 = "universe";
    std::string i10 = ")";
    std::string i11 = ")";
    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11};

    // "(hello&(world|meet&universe))"
    std::string input = i1 + i2 + i3 + i4 + i5 + i6 +  i7 + i8 + i9 + i10 + i11;
    std::vector<std::string_view> pack{parse_for_shunting(input)};

    std::cout << "expected ";
    print_vec(e);
    std::cout << "got ";
    print_vec(pack);

    ASSERT_THAT(e, testing::ElementsAreArray(pack));
  }

//  TEST(TestVecCompare, complex1_all_operators_no_space) {
//    std::string i1 = "(";
//    std::string i2 = "hello";
//    std::string i3 = "&";
//    std::string i4 = "(";
//    std::string i5 = "world";
//    std::string i6 = "|";
//    std::string i7 = "meet";
//    std::string i8 = "&";
//    std::string i9 = "universe";
//    std::string i10 = ")";
//    std::string i11 = ")";
//    std::vector<std::string_view> e = {i1, i2, i3, i4, i5, i6, i7, i8, i9, i10, i11};
//
//    // "(hello&(world|meet&universe))"
//    std::string input = i1 + i2 + i3 + i4 + i5 + i6 +  i7 + i8 + i9 + i10 + i11;
//    std::vector<std::string_view> pack{parse_for_shunting(input)};
//
//    std::cout << "expected ";
//    print_vec(e);
//    std::cout << "got ";
//    print_vec(pack);
//
//    ASSERT_THAT(e, testing::ElementsAreArray(pack));
//  }
}  // anon namespace