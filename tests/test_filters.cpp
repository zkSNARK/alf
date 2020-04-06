//
// Created by Christopher Goebel on 4/6/20.
//

#include "line_filter.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <numeric>


namespace
{
  using namespace alf::types;

  auto print_vec(std::vector<alf::types::TokenBase> const& v)
  {
    std::cout << "[";
    for (auto e = v.begin(); e != v.end(); ++e) {
      alf::types::TokenBase value = *e;
      std::cout << (e->required ? '+' : '-') << "'" << e->value
                << (e != v.end() - 1 ? "', " : "'");
    }
    std::cout << "]\n";
  }


  TEST(TestFilters, all_and_found)
  {
    std::string input_str = "have a bad cake day";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("b"), true }},
      { SubStr{ std::string("c"), true }},
      { SubStr{ std::string("d"), true }},
      { operators::AND{}},
      { operators::AND{}},
      { operators::AND{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                      input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, all_and_missing_b)
  {
    std::string input_str = "have a dad cake day";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("b"), true }},
      { SubStr{ std::string("c"), true }},
      { SubStr{ std::string("d"), true }},
      { operators::AND{}},
      { operators::AND{}},
      { operators::AND{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_FALSE(result);
  }

  TEST(TestFilters, all_and_required_missing_b)
  {
    std::string input_str = "have a dad cake day";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("b"), false }},
      { SubStr{ std::string("c"), true }},
      { SubStr{ std::string("d"), true }},
      { operators::AND{}},
      { operators::AND{}},
      { operators::AND{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, simple_or_a)
  {
    std::string input_str = "a";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("b"), true }},
      { operators::OR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, simple_or_b)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("b"), true }},
      { operators::OR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, simple_not_a_or_c)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), false }},
      { SubStr{ std::string("c"), true }},
      { operators::OR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, simple_a_or_not_c)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("c"), false }},
      { operators::OR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, simple_or_fail)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("c"), true }},
      { operators::OR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_FALSE(result);
  }

  TEST(TestFilters, simple_xor_fail)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("c"), true }},
      { operators::XOR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_FALSE(result);
  }

  TEST(TestFilters, simple_xor_fail_both_true)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), false }},
      { SubStr{ std::string("c"), false }},
      { operators::XOR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_FALSE(result);
  }

  TEST(TestFilters, simple_xor_first_true)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), false }},
      { SubStr{ std::string("c"), true }},
      { operators::XOR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }

  TEST(TestFilters, simple_xor_second_true)
  {
    std::string input_str = "b";
    std::vector<TokenBase> input_tokens{
      { SubStr{ std::string("a"), true }},
      { SubStr{ std::string("c"), false }},
      { operators::XOR{}}
    };

    std::queue<TokenBase, std::deque<TokenBase>> q(std::deque<TokenBase>(input_tokens.begin(),
                                                                         input_tokens.end()));
    auto result = alf::passes_filters(q, input_str);

    ASSERT_TRUE(result);
  }
}