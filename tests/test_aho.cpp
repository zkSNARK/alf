//
// The Aho-Corasick DFA must report exactly the occurrences a brute-force search finds: every
// string ending at every position, overlapping and nested ones included.
//

#include "aho_corasick.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace
{
  auto random_string(std::mt19937& rng, std::size_t len, char const* alphabet, std::size_t letters) -> std::string
  {
    std::string s;
    for (std::size_t i = 0; i < len; ++i) {
      s += alphabet[rng() % letters];
    }
    return s;
  }

  TEST(AhoCorasick, finds_every_occurrence_like_brute_force)
  {
    std::mt19937 rng(5);
    for (int round = 0; round < 2000; ++round) {
      // a tiny alphabet, so strings overlap and nest a lot ("a", "aa", "aba", ...)
      std::size_t const letters = 2 + rng() % 3;
      std::set<std::string> distinct;
      std::size_t const count = 1 + rng() % 40;
      while (distinct.size() < count) {
        distinct.insert(random_string(rng, 1 + rng() % 5, "ab\xffz", letters));
      }
      std::vector<std::string> const strings(distinct.begin(), distinct.end());
      std::vector<std::string_view> views(strings.begin(), strings.end());
      alf::detail::AhoCorasick const ac{ views };
      std::string const text = random_string(rng, rng() % 300, "ab\xffzq", letters + 1);

      std::vector<std::pair<std::size_t, std::uint32_t>> want, got;  // (end position, string)
      for (std::uint32_t i = 0; i < strings.size(); ++i) {
        for (std::size_t at = text.find(strings[i]); at != std::string::npos; at = text.find(strings[i], at + 1)) {
          want.emplace_back(at + strings[i].size() - 1, i);
        }
      }
      std::uint32_t state = alf::detail::AhoCorasick::start;
      for (std::size_t p = 0; p < text.size(); ++p) {
        state = ac.step(state, static_cast<unsigned char>(text[p]));
        if (ac.matches(state)) {
          for (std::uint32_t i : ac.ending(state)) {
            got.emplace_back(p, i);
          }
        }
      }
      std::sort(want.begin(), want.end());
      std::sort(got.begin(), got.end());
      ASSERT_EQ(got, want) << "round " << round << ", " << strings.size() << " strings, text '" << text << "'";
    }
  }

  TEST(AhoCorasick, rejects_no_strings_and_empty_strings)
  {
    EXPECT_THROW(alf::detail::AhoCorasick{ {} }, std::invalid_argument);
    EXPECT_THROW((alf::detail::AhoCorasick{ { "a", "" } }), std::invalid_argument);
  }
}
