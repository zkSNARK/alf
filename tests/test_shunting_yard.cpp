//
// Created by Christopher Goebel on 3/24/20.
//

#include "alf_types.h"
#include "shunting_yard.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <stack>
#include <string>

//#define VERBOSE

namespace
{
  using namespace alf::types;

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

  TEST(NegativeGroup, find_neg_group_with_no_space_start)
  {
    std::vector<std::string> v{"{", "re", "|", "abc", "}", "&", "(", "[", "par", "|", "m", "]", "&", "z", ")"};

    std::queue<std::string> que {shunting_yard(std::move(v))};


    while (!que.empty()) {
      std::cout << " " << que.front();
      que.pop();
    }
    std::cout << '\n';

    char const* input[] = { "dummy.exe", "-hello", "world", "whats", "up", "-i", "tests/infile.txt" };

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

//    alf::ArgPack result{ alf::parse_arguments(std::size(input), input) };

#ifdef VERBOSE
      std::cout << "expected  : ";
      print_vec(expected);
      std::cout << "actual    : ";
      print_vec(result.tokens);
#endif
//    ASSERT_THAT(result.tokens, testing::ElementsAreArray(expected));
  }
}