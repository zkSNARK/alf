#include "libalf.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <string>


namespace {
  template<typename T, int size>
  int array_size(T(&)[size]) {
    return size;
  }

// Tests arg_parse
  TEST(ArgParse, infile_at_beginning) {
    const char *test_args[3] = {"dummy.exe", "-i", "tests/infile.txt"};
    ArgPack pack{ parse_basic(array_size(test_args), test_args)};
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }

  TEST(ArgParse, infile_at_end) {
    const char *test_args[5] = {"dummy.exe", "-", "hello", "-i", "tests/infile.txt"};
    ArgPack pack{ parse_basic(array_size(test_args), test_args)};
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }

  TEST(ArgParse, infile_in_middle) {
    const char *test_args[7] = {"dummy.exe", "-", "hello", "-i", "tests/infile.txt", "+", "world"};
    ArgPack pack{ parse_basic(array_size(test_args), test_args)};
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }

  TEST(ArgParse, finds_one_neg_group_at_begin) {
    const char *test_args[5] = {"dummy.exe", "-", "hello", "-i", "tests/infile.txt"};
    ArgPack pack{ parse_basic(array_size(test_args), test_args)};
    ASSERT_EQ(pack.infile, "tests/infile.txt");
  }


  // TEST NEGATIVE CASES
  TEST(TestVecCompare, find_neg_group_with_no_space_start) {
    std::string e1 = "-hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e2, e3, e4};
    v.insert(v.begin(), "hello");
    const char *args[7] = {"dummy.exe", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str(), "-i", "tests/infile.txt"};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.neg[0]));
  }

  TEST(TestVecCompare, find_neg_group_at_begin_mult_elements) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e1, e2, e3, e4};
    const char *args[8] = {"dummy.exe", "-", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str(), "-i", "tests/infile.txt"};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.neg[0]));
  }

  TEST(TestVecCompare, find_neg_group_at_end_mult_elements) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e1, e2, e3, e4};
    const char *args[8] = {"dummy.exe", "-i", "tests/infile.txt", "-", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str()};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.neg[0]));
  }

  TEST(TestVecCompare, find_neg_group_in_middle_mult_elements) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e1, e2, e3, e4};
    const char *args[10] = {"dummy.exe", "-i", "tests/infile.txt", "-", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str(), "-o", "dummy.out"};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.neg[0]));
  }


  // TEST POSITIVE CASES
  TEST(TestVecCompare, find_pos_group_with_no_space_start) {
    std::string e1 = "+hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e2, e3, e4};
    v.insert(v.begin(), "hello");
    const char *args[7] = {"dummy.exe", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str(), "-i", "tests/infile.txt"};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.pos[0]));
  }

  TEST(TestVecCompare, find_pos_group_at_begin_mult_elements) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e1, e2, e3, e4};
    const char *args[8] = {"dummy.exe", "+", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str(), "-i", "tests/infile.txt"};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.pos[0]));
  }

  TEST(TestVecCompare, find_pos_group_at_end_mult_elements) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e1, e2, e3, e4};
    const char *args[8] = {"dummy.exe", "-i", "tests/infile.txt", "+", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str()};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.pos[0]));
  }

  TEST(TestVecCompare, find_pos_group_in_middle_mult_elements) {
    std::string e1 = "hello";
    std::string e2 = "world";
    std::string e3 = "whats";
    std::string e4 = "up";
    std::vector v{e1, e2, e3, e4};
    const char *args[10] = {"dummy.exe", "-i", "tests/infile.txt", "+", e1.c_str(), e2.c_str(), e3.c_str(), e4.c_str(), "-o", "dummy.out"};

    ArgPack pack{ parse_basic(array_size(args), args)};
    ASSERT_THAT(v, testing::ElementsAreArray(pack.pos[0]));
  }
}  // anon namespace