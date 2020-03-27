

#include "libalf.h"
#include "shunting_yard.h"

#include <fcntl.h>
#include <filesystem>
#include <iostream>

namespace {
  auto is_quote(char c) -> bool {
    return c == '\'' or c == '"';
  }

  auto is_operator(char c) -> bool {
    return c == '&' or c == '|';
  }

  auto _push(std::vector<std::string> &v, std::string_view sv, bool last_is_substr) {
    if (last_is_substr) {
      v.emplace_back("&");
    }
    v.emplace_back(sv);
  }
}

/**
 * Parse the input args into a vector, splitting tokens out, and either prefixing
 * strings with either + or -.
 * @param s
 * @return
 */
auto parse_for_shunting(std::string_view const &s) -> std::vector<std::string> {

  std::stack<char> paren_stk;
  std::stack<char> quote_stk;
  std::string_view const delims = " '\"&|{([])}";
  std::vector<std::string> output;
  output.reserve(s.size() / 2);

  bool last_is_substr = false;
  for (auto first = s.data(), second = s.data(), last = first + s.size();
       second != last && first != last; first = second + 1) {

    second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

    // if the current location is a delimiter other than a space
    bool push_later = false;
    char c = *second;

    if (is_quote(c)) {
      first = second + 1;
      std::string quote{c};
      second = std::find_first_of(first, last, std::cbegin(quote), std::cend(quote));
      if (second == last) {
        throw std::runtime_error("imperfect quotes");
      }
    }
    if (is_opening_paren(c)) {
      paren_stk.push(c);
      if (second == s.data()) {
        _push(output, std::string_view(second, 1), last_is_substr);
      } else {
        push_later = true;
      }
    }

    // common case -> regular words
    if (first != second) {
      _push(output, std::string_view(first, second - first), last_is_substr);
      last_is_substr = true;
    }

    if (push_later) {
      _push(output, std::string_view(second, 1), last_is_substr);
      last_is_substr = false;
    }
    if (is_operator(c)) {
      output.emplace_back(second, 1);
      last_is_substr = false;
    }
    if (is_closing_paren(c)) {
      if (paren_stk.top() != companion_parenthesis(c)) {
        throw std::runtime_error("detected imbalanced or unmatched parenthesis during parsing");
      }
      output.emplace_back(second, 1);
      last_is_substr = false;
    }
  }

  return output;
}


/**
 *
 * @param argc
 * @param argv
 * @return
 */
auto parse_args(int argc, const char **argv) -> std::tuple<FileNameOpt, FileNameOpt, FilterPack, FilterPack> {
  std::optional<std::string> infile;
  std::optional<std::string> outfile;
  FilterPack and_positive;
  FilterPack and_negative;

  for (int i{1}; i < argc; ++i) {
    std::string arg{argv[i]};
    if (arg == "-o" or arg == "--outfile") {
      if (i + 1 >= argc) {
        throw std::runtime_error("-o or --outfile must be followed by a valid file name");
      }
      outfile = argv[i + 1];
      ++i;
      continue;
    }
    if (arg == "-i" or arg == "--infile") {
      if (i + 1 >= argc) {
        throw std::runtime_error("-i or --infile must be followed by a valid file name");
      }
      infile = argv[i + 1];
      if (!std::filesystem::exists(*infile)) {
        throw std::runtime_error("infile : " + *infile + " does not exist");
      }
      ++i;
      continue;
    }

    // If a string is not prefixed, it is considered a positive requirement.
    // Accepts strings starting with '+' or otherwise not starting with '-' prefix
    if (arg[0] != '-') {
      std::vector<std::string> positive_and_group{};
      if (arg == "+") {
      } else if (arg[0] == '+' and
                 arg.size() > 1) { // if the user input a string like "+hello", this will strip off the +
        positive_and_group.emplace_back(argv[i] + 1);
      } else { // handles args with no '+' or '-' prefix
        positive_and_group.emplace_back(arg);
      }
      while (i + 1 < argc and strncmp(argv[i + 1], "+", 1) != 0 and strncmp(argv[i + 1], "-", 1) != 0) {
        positive_and_group.emplace_back(argv[i + 1]);
        ++i;
      }
      and_positive.emplace_back(std::move(positive_and_group));
      continue;
    }

    if (arg[0] == '-') {
      std::vector<std::string> negative_and_group{};
      if (arg.size() > 1) { // if the user input a string like "-hello", this will strip off the -
        negative_and_group.emplace_back(argv[i] + 1);
      }
      while (i + 1 < argc and strncmp(argv[i + 1], "+", 1) != 0 and strncmp(argv[i + 1], "-", 1) != 0) {
        negative_and_group.emplace_back(argv[i + 1]);
        ++i;
      }
      and_negative.emplace_back(std::move(negative_and_group));
      continue;
    }
  }

  return {infile, outfile, and_positive, and_negative};
}


auto passes_filters(FilterPack const &fp_pos, FilterPack const &fp_neg, std::string const &line) -> bool {
  for (std::vector<std::string> const &or_pack : fp_pos) {
    bool or_pack_satisfied = false;
    for (std::string const &s : or_pack) {
      if (line.find(s) != std::string::npos) {
        or_pack_satisfied = true;
        break;
      }
    }
    if (!or_pack_satisfied) {
      return false;
    }
  }

  for (std::vector<std::string> const &or_pack : fp_neg) {
    bool or_pack_satisfied = false;
    for (std::string const &s : or_pack) {
      if (line.find(s) == std::string::npos) {
        or_pack_satisfied = true;
        break;
      }
    }
    if (!or_pack_satisfied) {
      return false;
    }
  }

  return true;
}


auto apply_filters(ArgPack p) -> void {
  FILE *fi = p.infile ? fopen(p.infile->c_str(), "r") : stdin;
  FILE *fo = p.outfile ? fopen(p.outfile->c_str(), "w+") : stdout;

  if (fi == nullptr or fo == nullptr) {
    exit(EXIT_FAILURE);
  }

  char *line = nullptr;
  size_t len = 0;
  while ((getline(&line, &len, fi)) != -1) {
    if (passes_filters(p.pos, p.neg, line)) {
      fputs(line, fo);
    }
  }
  fclose(fi);
  fclose(fo);

  if (line) {
    free(line);
  }
}



