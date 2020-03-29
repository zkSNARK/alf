//
// Created by Christopher Goebel on 3/21/20.
//

#include "libalf.h"

#include <fcntl.h>
#include <filesystem>
#include <string>
#include <vector>
#include <stack>

namespace {
  bool is_closing_bracket(alf::types::TYPE_TOKEN type) {
    return type == alf::types::TYPE_TOKEN::BRACKET_CLOSE_CURLY
           or type == alf::types::TYPE_TOKEN::BRACKET_CLOSE_SQUARE
           or type == alf::types::TYPE_TOKEN::BRACKET_CLOSE_PAREN;
  }

  bool is_binary_operator(alf::types::TYPE_TOKEN type) {
    return type == alf::types::TYPE_TOKEN::OPERATOR_AND
           or type == alf::types::TYPE_TOKEN::OPERATOR_OR
           or type == alf::types::TYPE_TOKEN::OPERATOR_XOR;
  }

  auto is_quote(char c) -> bool {
    return c == '\'' or c == '"';
  }


  auto is_unary_operator(char c) -> bool {
    return c == '+' or c == '-';
  }


  auto is_binary_operator(char c) -> bool {
    return c == '&' or c == '|';
  }


  auto _push(std::vector<std::string> &v, std::string_view sv, bool last_is_substr) {
    if (last_is_substr) {
      v.emplace_back("&");
    }
    if (v.back().size() == 1 and !is_unary_operator(v.back()[0])) {
      v.emplace_back("+");
    }
    v.emplace_back(sv);
  }


  auto build_token(std::string_view token, bool& is_pos_req, std::vector<alf::types::TokenBase>& tokens)
  {
    switch (token[0]) {
      case '|': {
        tokens.emplace_back(alf::types::operators::OR{});
        is_pos_req = true;
        break;
      }
      case '&': {
        tokens.emplace_back(alf::types::operators::AND{});
        is_pos_req = true;
        break;
      }
      case '^': {
        tokens.emplace_back(alf::types::operators::XOR{});
        is_pos_req = true;
        break;
      }
      case '+': {
        is_pos_req = true;
        break;
      }
      case '-': {
        is_pos_req = false;
        break;
      }
      case '(': {
        tokens.emplace_back(alf::types::brackets::OpenParen{});
        is_pos_req = true;
        break;
      }
      case '[': {
        tokens.emplace_back(alf::types::brackets::OpenSquareBracket{});
        is_pos_req = true;
        break;
      }
      case '{': {
        tokens.emplace_back(alf::types::brackets::OpenCurlyBracket{});
        is_pos_req = true;
        break;
      }
      case ')': {
        tokens.emplace_back(alf::types::brackets::CloseParen{});
        is_pos_req = true;
        break;
      }
      case ']': {
        tokens.emplace_back(alf::types::brackets::CloseSquareBracket{});
        is_pos_req = true;
        break;
      }
      case '}': {
        tokens.emplace_back(alf::types::brackets::CloseCurlyBracket{});
        is_pos_req = true;
        break;
      }
      default:
        tokens.emplace_back(alf::types::SubStr{token, is_pos_req});
        is_pos_req = true;
        break;
    }
  }



  auto handle_quote(char const*& first, char const*& second, char const*& last, std::vector<alf::types::TokenBase>& tokens, bool& is_pos_req) {
    static std::string const single_quote = "'";
    static std::string const double_quote = "\"";

    first = second + 1;
    switch (*second) {
      case '\'': {
          second = std::find_first_of(first, last, std::cbegin(single_quote), std::cend(single_quote));
          if(second == last) {
            throw std::runtime_error("imperfect quotes");
          }

          std::string_view sv(first, second - first);
          tokens.emplace_back(alf::types::SubStr{sv, is_pos_req});
          is_pos_req = true;
        }
        break;
      case '"': {
          second = std::find_first_of(first, last, std::cbegin(double_quote), std::cend(double_quote));
          if(second == last) {
            throw std::runtime_error("imperfect quotes");
          }

          std::string_view sv(first, second - first);
          tokens.emplace_back(alf::types::SubStr{sv, is_pos_req});
          is_pos_req = true;
        }
        break;
    }
  }


  std::vector<alf::types::TokenBase> fill_in_missing_AND_symbols(std::vector<alf::types::TokenBase>&v) {
    std::vector<alf::types::TokenBase> tokens;
    tokens.reserve(v.size() * 2);

    bool last_requires_binary_operator = false;
    for (alf::types::TokenBase& t : v) {
      if (last_requires_binary_operator && !is_binary_operator(t.type)) {
        tokens.emplace_back(alf::types::operators::AND{});
        last_requires_binary_operator = false;
      }
      if (t.type == alf::types::TYPE_TOKEN::SUBSTR or is_closing_bracket(t.type)) {
        last_requires_binary_operator = true;
      }
      tokens.emplace_back(std::move(t));
    }
    return tokens;
  }
}


namespace alf {

  /**
   * Parse the input args into a vector, splitting tokens out, and either prefixing
   * strings with either + or -.
   *
   * This parser prepares the input for the shunting yard algorithm.
   *
   * @param s
   * @return
   */
  auto parse_algebraic(std::string_view const& s) -> std::vector<alf::types::TokenBase>
  {
    static std::string_view const delims = " '\"&|{([])}-+";
    std::stack<char> paren_stk;
    std::vector<alf::types::TokenBase> tokens;
    tokens.reserve(s.size() / 2);

    bool is_positive_req = true;

    for (auto first = s.data(), second = s.data(), last = first + s.size();
         second != last && first != last; first = second + 1)
    {
      second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

      if (first != second) {
        build_token(std::string_view(first, second - first), is_positive_req, tokens);
      }
      if (is_quote(*second)) {
        handle_quote(first, second, last, tokens, is_positive_req);
      } else if (*second != ' ' and second != last) {
        build_token(std::string_view{second, 1}, is_positive_req, tokens);
      }
    }

    return tokens;
  }


  /**
   *
   * @param argc
   * @param argv
   * @return
   */
  auto parse_basic(int argc, const char **argv) -> std::tuple<FileNameOpt, FileNameOpt, FilterPack, FilterPack> {
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
} // end namespace alf
