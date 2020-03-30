#include "libalf.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>


namespace
{

  auto print_vec(std::vector<alf::types::TokenBase>& v)
  {
    std::cout << "[";
    for (auto e = v.begin(); e != v.end(); ++e) {
      alf::types::TokenBase value = *e;
      std::cout << (e->required ? '+' : '-') << "'" << e->value
                << (e != v.end() - 1 ? "', " : "'");
    }
    std::cout << "]\n";
  }

  using namespace alf::types;

  template<typename T, int size>
  int array_size(T(&)[size])
  {
    return size;
  }

// Tests arg_parse
  TEST(FindInfile, infile_at_beginning)
  {
    const char* test_args[3] = { "dummy.exe", "-i", "tests/infile.txt" };
    alf::ArgPack pack{ alf::parse_arguments(array_size(test_args), test_args) };
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }

  TEST(FindInfile, infile_in_middle)
  {
    const char* test_args[7] = { "dummy.exe", "-", "hello", "-i", "tests/infile.txt", "+", "world" };
    alf::ArgPack pack{ alf::parse_arguments(array_size(test_args), test_args) };
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }

  TEST(FindInfile, infile_at_end)
  {
    const char* test_args[5] = { "dummy.exe", "-", "hello", "-i", "tests/infile.txt" };
    alf::ArgPack pack{ alf::parse_arguments(array_size(test_args), test_args) };
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }


  TEST(FindOutfile, outfile_at_beginning)
  {
    const char* test_args[3] = { "dummy.exe", "-o", "tests/outfile.txt" };
    alf::ArgPack pack{ alf::parse_arguments(array_size(test_args), test_args) };
    ASSERT_EQ(pack.outfile, "tests/outfile.txt");
  }

  TEST(FindOutfile, outfile_in_middle)
  {
    const char* test_args[7] = { "dummy.exe", "-", "hello", "-o", "tests/outfile.txt", "+", "world" };
    alf::ArgPack pack{ alf::parse_arguments(array_size(test_args), test_args) };
    ASSERT_EQ(pack.outfile, "tests/outfile.txt");
  }

  TEST(FindOutfile, outfile_at_end)
  {
    const char* test_args[5] = { "dummy.exe", "-", "hello", "-o", "tests/outfile.txt" };
    alf::ArgPack pack{ alf::parse_arguments(array_size(test_args), test_args) };
    ASSERT_EQ(pack.outfile, "tests/outfile.txt");
  }




  // TEST NEGATIVE CASES
  TEST(NegativeGroup, find_neg_group_with_no_space_start)
  {
    std::vector<std::string> v = {
      "dummy.exe", "-hello", "world", "whats", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), false }},
      { operators::OR{}},
      { SubStr{ std::string("world"), false }},
      { operators::OR{}},
      { SubStr{ std::string("whats"), false }},
      { operators::OR{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(NegativeGroup, find_neg_group_after_other_arg_with_no_space_start)
  {
    std::vector<std::string> v = {
      "dummy.exe", "-i", "tests/infile.txt", "-hello", "world", "whats", "up"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), false }},
      { operators::OR{}},
      { SubStr{ std::string("world"), false }},
      { operators::OR{}},
      { SubStr{ std::string("whats"), false }},
      { operators::OR{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(NegativeGroup, find_neg_group_in_middle_of_other_args_with_no_space_start)
  {
    std::vector<std::string> v = {
      "dummy.exe", "-i", "tests/infile.txt", "-hello", "world", "whats", "up", "-o", "outfile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), false }},
      { operators::OR{}},
      { SubStr{ std::string("world"), false }},
      { operators::OR{}},
      { SubStr{ std::string("whats"), false }},
      { operators::OR{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(NegativeGroup, neg_group_mid_starting_pos)
  {
    std::vector<std::string> v = {
      "dummy.exe", "hello", "world", "-whats", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { SubStr{ std::string("hello"), true }},
      { SubStr{ std::string("world"), true }},
      { brackets::OpenParen{}},
      { SubStr{ std::string("whats"), false }},
      { operators::OR{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(NegativeGroup, find_neg_group_with_space_at_start)
  {
    std::vector<std::string> v = {
      "dummy.exe", "-", "hello", "world", "whats", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), false }},
      { operators::OR{}},
      { SubStr{ std::string("world"), false }},
      { operators::OR{}},
      { SubStr{ std::string("whats"), false }},
      { operators::OR{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(NegativeGroup, all_negative_groups_no_space)
  {
    std::vector<std::string> v = {
      "dummy.exe", "-hello", "-world", "-whats", "-up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), false }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("world"), false }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("whats"), false }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(NegativeGroup, all_negative_groups_w_space)
  {
    std::vector<std::string> v = {
      "dummy.exe", "-", "hello", "-", "world", "-", "whats", "-", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), false }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("world"), false }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("whats"), false }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("up"), false }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }


  // TEST BASIC POSITIVE CASES
  TEST(PositiveGroup, pos_group_with_no_space_start)
  {
    // When tokens are not prefixed by a '+' or '-', they are not considered to be
    // part of any groups, so they are simply parsed as required substrings with
    // an implied AND '&' between them.  This is a special case for the most basic
    // usage pattern similar to grep
    std::vector<std::string> v = {
      "dummy.exe", "hello", "world", "whats", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { SubStr{ std::string("hello"), true }},
      { SubStr{ std::string("world"), true }},
      { SubStr{ std::string("whats"), true }},
      { SubStr{ std::string("up"), true }},
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(PositiveGroup, pos_group_after_2_no_group)
  {
    std::vector<std::string> v = {
      "dummy.exe", "hello", "world", "+whats", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { SubStr{ std::string("hello"), true }},
      { SubStr{ std::string("world"), true }},
      { brackets::OpenParen{}},
      { SubStr{ std::string("whats"), true }},
      { operators::OR{}},
      { SubStr{ std::string("up"), true }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(PositiveGroup, find_pos_group_with_space_at_start)
  {
    std::vector<std::string> v = {
      "dummy.exe", "+", "hello", "world", "whats", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), true }},
      { operators::OR{}},
      { SubStr{ std::string("world"), true }},
      { operators::OR{}},
      { SubStr{ std::string("whats"), true }},
      { operators::OR{}},
      { SubStr{ std::string("up"), true }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(PositiveGroup, all_pos_groups_no_space)
  {
    std::vector<std::string> v = {
      "dummy.exe", "+hello", "+world", "+whats", "+up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), true }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("world"), true }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("whats"), true }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("up"), true }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }

  TEST(PositiveGroup, all_pos_groups_w_space)
  {
    std::vector<std::string> v = {
      "dummy.exe", "+", "hello", "+", "world", "+", "whats", "+", "up", "-i", "tests/infile.txt"
    };
    std::vector<char*> input;
    input.reserve(v.size());
    for (auto& e : v) {
      input.emplace_back(e.data());
    }
    std::vector<TokenBase> expected{
      { brackets::OpenParen{}},
      { SubStr{ std::string("hello"), true }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("world"), true }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("whats"), true }},
      { brackets::CloseParen{}},
      { brackets::OpenParen{}},
      { SubStr{ std::string("up"), true }},
      { brackets::CloseParen{}}
    };

    alf::ArgPack result{ alf::parse_arguments(input.size(), const_cast<const char**>(input.data())) };

    std::cout << "expected  : ";
    print_vec(expected);
    std::cout << "actual    : ";
    print_vec(result.tokens);
    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }
}  // anon namespace