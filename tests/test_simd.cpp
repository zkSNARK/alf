//
// Every SIMD backend that runs on this machine must return exactly what the scalar backend
// returns, on random inputs with random offsets, lengths and read limits.
//

#include "simd.h"

#include <gtest/gtest.h>

#include <random>
#include <string>
#include <vector>

namespace
{
  using alf::simd::Backend;

  auto runnable() -> std::vector<Backend const*>
  {
    std::vector<Backend const*> out;
    for (Backend const* b : alf::simd::compiled()) {
      if (b->runs_here()) {
        out.push_back(b);
      }
    }
    return out;
  }

  auto scalar() -> Backend const& { return *alf::simd::by_name("scalar"); }

  // Text with a given density of "interesting" bytes, so kernels see both long empty runs and
  // matches in every lane position.
  auto make_text(std::mt19937& rng, std::size_t n, unsigned density) -> std::string
  {
    std::string s(n, 'x');
    for (auto& c : s) {
      unsigned const r = rng() % 1000;
      c = r < density ? "ab\nc"[rng() % 4] : static_cast<char>('d' + rng() % 20);
    }
    return s;
  }

  TEST(Simd, backends_are_listed)
  {
    ASSERT_NE(alf::simd::by_name("scalar"), nullptr);
    EXPECT_TRUE(alf::simd::active().runs_here());
    for (Backend const* b : runnable()) {
      std::cout << "  backend runs here: " << b->name << (b == &alf::simd::active() ? " (active)" : "") << "\n";
    }
  }

  TEST(Simd, find_any_matches_scalar)
  {
    std::mt19937 rng(1);
    for (int round = 0; round < 3000; ++round) {
      std::string const text = make_text(rng, rng() % 700, 1 + rng() % 60);
      alf::simd::ByteSet set{};
      unsigned const k = 1 + rng() % 5;
      for (unsigned i = 0; i < k; ++i) {
        auto const b = static_cast<unsigned char>("abcdz\n"[rng() % 6]);
        if (!set.member[b]) {
          set.member[b] = true;
          set.bytes[set.count++] = b;
        }
      }
      std::size_t const from = text.empty() ? 0 : rng() % (text.size() + 1);
      std::size_t const to = from + (text.size() > from ? rng() % (text.size() - from + 1) : 0);
      char const* base = text.data();
      for (Backend const* b : runnable()) {
        ASSERT_EQ(b->find_any(set, base + from, base + to, base + text.size()),
                  scalar().find_any(set, base + from, base + to, base + text.size()))
          << b->name << " round " << round;
      }
    }
  }

  TEST(Simd, teddy_matches_scalar)
  {
    std::mt19937 rng(2);
    for (int round = 0; round < 3000; ++round) {
      std::string const text = make_text(rng, rng() % 700, 1 + rng() % 300);
      alf::simd::TeddyTables t{};
      // random sparse tables: each group allows a few half-byte values at each position
      for (int k = 0; k < 3; ++k) {
        for (int g = 0; g < 8; ++g) {
          for (int j = 0; j < 2; ++j) {
            t.lo[k][rng() % 16] |= static_cast<unsigned char>(1u << g);
            t.hi[k][6 + rng() % 2] |= static_cast<unsigned char>(1u << g);  // 0x6_ and 0x7_: lowercase letters
          }
        }
      }
      std::size_t const from = text.empty() ? 0 : rng() % (text.size() + 1);
      std::size_t const to = from + (text.size() > from ? rng() % (text.size() - from + 1) : 0);
      char const* base = text.data();
      for (Backend const* b : runnable()) {
        ASSERT_EQ(b->teddy(t, base + from, base + to, base + text.size()),
                  scalar().teddy(t, base + from, base + to, base + text.size()))
          << b->name << " round " << round;
      }
    }
  }

  TEST(Simd, find_pair_matches_scalar)
  {
    std::mt19937 rng(4);
    for (int round = 0; round < 4000; ++round) {
      std::string const text = make_text(rng, rng() % 700, 1 + rng() % 400);
      alf::simd::BytePair pair{};
      pair.a = static_cast<unsigned char>("abc\n"[rng() % 4]);
      pair.b = static_cast<unsigned char>("abc\n"[rng() % 4]);
      pair.offset = 1 + rng() % 40;
      std::size_t const from = text.empty() ? 0 : rng() % (text.size() + 1);
      std::size_t const to = from + (text.size() > from ? rng() % (text.size() - from + 1) : 0);
      std::size_t const limit = to + (text.size() > to ? rng() % (text.size() - to + 1) : 0);  // reading stops anywhere past `last`
      char const* base = text.data();
      for (Backend const* b : runnable()) {
        ASSERT_EQ(b->find_pair(pair, base + from, base + to, base + limit),
                  scalar().find_pair(pair, base + from, base + to, base + limit))
          << b->name << " round " << round << " offset " << pair.offset;
      }
    }
  }

  TEST(Simd, line_start_matches_scalar)
  {
    std::mt19937 rng(3);
    for (int round = 0; round < 3000; ++round) {
      std::string const text = make_text(rng, 1 + rng() % 700, 1 + rng() % 40);
      std::size_t const at = rng() % (text.size() + 1);
      std::size_t const floor = at == 0 ? 0 : rng() % (at + 1);
      char const* base = text.data();
      for (Backend const* b : runnable()) {
        ASSERT_EQ(b->line_start(base + at, base + floor), scalar().line_start(base + at, base + floor))
          << b->name << " round " << round;
      }
    }
  }

  TEST(Simd, forcing_a_backend)
  {
    Backend const& was = alf::simd::active();
    for (Backend const* b : runnable()) {
      alf::simd::use(*b);
      EXPECT_EQ(&alf::simd::active(), b);
    }
    alf::simd::use(was);
  }
}
