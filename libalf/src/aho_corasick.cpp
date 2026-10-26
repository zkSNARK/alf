//
// aho_corasick.cpp -- building the DFA (see aho_corasick.h).
//
// The textbook construction: a trie of the strings; breadth-first, each node's failure link (the
// longest proper suffix of its path that is also a trie path) and its full transition row
// (a missing child is the failure node's transition, already computed one level up); each node's
// matches are its own strings plus its failure node's.  Then the states are renumbered so that
// match states come last, and the table is laid out with row offsets as ids.
//

#include "aho_corasick.h"

#include <cstddef>
#include <stdexcept>

alf::detail::AhoCorasick::AhoCorasick(std::vector<std::string_view> const& strings, bool fold)
{
  auto const key = [fold](char c) -> unsigned char {
    auto const b = static_cast<unsigned char>(c);
    return (fold and b >= 'A' and b <= 'Z') ? static_cast<unsigned char>(b - 'A' + 'a') : b;
  };
  if (strings.empty()) {
    throw std::invalid_argument("AhoCorasick: no strings");
  }

  // byte classes
  for (std::string_view s : strings) {
    if (s.empty()) {
      throw std::invalid_argument("AhoCorasick: empty string");
    }
    for (char c : s) {
      auto const b = key(c);
      if (m_class[b] == 0) {
        m_class[b] = m_classes++;
      }
    }
  }
  if (fold) {  // an upper-case letter in the text steps like its lower-case one
    for (unsigned char b = 'A'; b <= 'Z'; ++b) {
      m_class[b] = m_class[b - 'A' + 'a'];
    }
  }
  std::size_t const k = m_classes;

  // trie, rows indexed by class; 0 = no child yet (the root, node 0, is never anyone's child)
  std::vector<std::uint32_t> child(k, 0);
  std::vector<std::vector<std::uint32_t>> own(1);  // strings ending exactly at each node
  for (std::uint32_t i = 0; i < strings.size(); ++i) {
    std::uint32_t node = 0;
    for (char c : strings[i]) {
      std::uint32_t const cls = m_class[key(c)];
      if (child[node * k + cls] == 0) {
        child[node * k + cls] = static_cast<std::uint32_t>(own.size());
        child.resize(child.size() + k, 0);
        own.emplace_back();
      }
      node = child[node * k + cls];
    }
    own[node].push_back(i);
  }
  std::size_t const nodes = own.size();

  // breadth-first: failure links, full transitions (in `child`, now complete), matches
  std::vector<std::uint32_t> fail(nodes, 0), order;
  std::vector<std::vector<std::uint32_t>> out(nodes);
  order.reserve(nodes);
  order.push_back(0);
  for (std::size_t cls = 0; cls < k; ++cls) {
    if (std::uint32_t const v = child[cls]; v != 0) {
      fail[v] = 0;
      order.push_back(v);
    }  // a missing child of the root stays 0: back to the root
  }
  for (std::size_t at = 1; at < order.size(); ++at) {
    std::uint32_t const u = order[at];
    out[u] = own[u];
    out[u].insert(out[u].end(), out[fail[u]].begin(), out[fail[u]].end());
    for (std::size_t cls = 0; cls < k; ++cls) {
      std::uint32_t& to = child[u * k + cls];
      if (to != 0) {
        fail[to] = child[fail[u] * k + cls];
        order.push_back(to);
      } else {
        to = child[fail[u] * k + cls];
      }
    }
  }

  // renumber: states without matches first (the root stays 0), then match states
  std::vector<std::uint32_t> id(nodes);
  std::uint32_t next_id = 0;
  for (std::size_t pass = 0; pass < 2; ++pass) {
    for (std::size_t u = 0; u < nodes; ++u) {
      if (out[u].empty() == (pass == 0)) {
        id[u] = next_id++;
      }
    }
  }
  std::uint32_t first_match_index = 0;
  for (std::size_t u = 0; u < nodes; ++u) {
    first_match_index += out[u].empty() ? 1u : 0u;
  }
  m_first_match = first_match_index * m_classes;

  m_next.assign(nodes * k, 0);
  m_out_at.assign(nodes - first_match_index + 1, 0);
  for (std::size_t u = 0; u < nodes; ++u) {
    for (std::size_t cls = 0; cls < k; ++cls) {
      m_next[id[u] * k + cls] = id[child[u * k + cls]] * m_classes;
    }
  }
  std::vector<std::size_t> match_node(nodes - first_match_index);
  for (std::size_t u = 0; u < nodes; ++u) {
    if (!out[u].empty()) {
      match_node[id[u] - first_match_index] = u;
    }
  }
  for (std::size_t m = 0; m < match_node.size(); ++m) {
    auto const& o = out[match_node[m]];
    m_out.insert(m_out.end(), o.begin(), o.end());
    m_out_at[m + 1] = static_cast<std::uint32_t>(m_out.size());
  }
}
