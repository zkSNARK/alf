//
// Created by Christopher Goebel on 3/21/20.
//

#include "parser.h"

#include <filesystem>
#include <string>
#include <vector>
#include <stack>
#include <cstring>
#include <stdexcept>

namespace
{
  auto is_quote(char c) -> bool
  {
    return c == '\'' or c == '"';
  }


  auto make_token(std::string_view token, bool& is_pos_req, std::vector<alf::types::TokenBase>& tokens)
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
        tokens.emplace_back(alf::types::SubStr{ token, is_pos_req });
        is_pos_req = true;
        break;
    }
  }


  auto
  handle_quote(char const*& first, char const*& second, char const*& last,
               std::vector<alf::types::TokenBase>& tokens, bool& is_pos_req)
  {
    static std::string const single_quote = "'";
    static std::string const double_quote = "\"";

    first = second + 1;
    switch (*second) {
      case '\'': {
        second = std::find_first_of(first, last, std::cbegin(single_quote), std::cend(single_quote));
        if (second == last) {
          throw std::runtime_error("imperfect quotes");
        }

        std::string_view sv(first, second - first);
        tokens.emplace_back(alf::types::SubStr{ sv, is_pos_req });
        is_pos_req = true;
      }
        break;
      case '"': {
        second = std::find_first_of(first, last, std::cbegin(double_quote), std::cend(double_quote));
        if (second == last) {
          throw std::runtime_error("imperfect quotes");
        }

        std::string_view sv(first, second - first);
        tokens.emplace_back(alf::types::SubStr{ sv, is_pos_req });
        is_pos_req = true;
      }
        break;
    }
  }

  void add_or_pack(std::vector<alf::types::TokenBase>& tokens, std::vector<alf::types::TokenBase> v)
  {
    tokens.emplace_back(alf::types::brackets::OpenParen{});
    for (auto e = v.begin(); e != v.end(); ++e) {

      tokens.emplace_back(std::move(*e));
      if (e != v.end() - 1) {
        tokens.emplace_back(alf::types::operators::OR{});
      }
    }
    tokens.emplace_back(alf::types::brackets::CloseParen{});
  }

  void add_alg_pack(std::vector<alf::types::TokenBase>& tokens, std::vector<alf::types::TokenBase> v)
  {
    tokens.emplace_back(alf::types::brackets::OpenParen{});
    tokens.insert(std::end(tokens), std::begin(v), std::end(v));
    tokens.emplace_back(alf::types::brackets::CloseParen{});
  }
}


namespace alf::parser
{

  /**
   * Since the user is allowed to enter statements like 'a b' and then is supposed to mean
   * require substr 'a' AND require substr 'b', but the postfix converter wants to see all
   * binary operators in place, we need to fill in those operators.  We could do this inside
   * of the parser for extra speed, but for now I am keeping this separate.  I believe this
   * is the correct thing to do because the parsing and this fill in together are fast enough
   * for expected input sizes that the 2 phase parsing and fill in will probably never be of
   * concern compared to the actual search phase.  That could be wrong is someone comes up
   * with some million token algebra for their requirements, but even then I don't think this
   * would cost much time.
   *
   * Small state machine which scans the parsed tokens and fills in implied '&' tokens.
   *
   * @param v
   * @return
   */
  std::vector<alf::types::TokenBase> fill_in_missing_AND_symbols(std::vector<alf::types::TokenBase>& v)
  {
    std::vector<alf::types::TokenBase> tokens;
    tokens.reserve(v.size() * 2);

    alf::types::TYPE_TOKEN previous_type = alf::types::TYPE_TOKEN::OPERATOR_AND;
    for (types::TokenBase& t : v) {
      alf::types::TYPE_TOKEN cur_type = t.type;
      if (cur_type == alf::types::TYPE_TOKEN::SUBSTR and
          (previous_type == alf::types::TYPE_TOKEN::SUBSTR or is_closing_bracket(previous_type))) {
        tokens.emplace_back(alf::types::operators::AND{});
      } else if (is_opening_bracket(cur_type) and
                 (previous_type == alf::types::TYPE_TOKEN::SUBSTR or is_closing_bracket(previous_type))) {
        tokens.emplace_back(alf::types::operators::AND{});
      }
      tokens.emplace_back(std::move(t));
      previous_type = cur_type;
    }
    return std::move(tokens);
  }

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
         second != last && first != last; first = second + 1) {
      second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

      if (first != second) {
        make_token(std::string_view(first, second - first), is_positive_req, tokens);
      }
      if (is_quote(*second)) {
        handle_quote(first, second, last, tokens, is_positive_req);
      } else if (*second != ' ' and second != last) {
        make_token(std::string_view{ second, 1 }, is_positive_req, tokens);
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
  auto parse_arguments(int argc, const char** argv) -> parser::ArgPack
  {
    std::optional<std::string> infile;
    std::optional<std::string> outfile;
    std::vector<alf::types::TokenBase> tokens;

    for (int i{ 1 }; i < argc; ++i) {
      std::string arg{ argv[i] };
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
      if (arg == "-a" or arg == "--algebra") {
        if (i + 1 >= argc) {
          throw std::runtime_error("-a or --algebra must be followed by set of algebraic requirement expression");
        }
        add_alg_pack(tokens, parse_algebraic(argv[i + 1]));
        ++i;
        continue;
      }

      // If a string is not prefixed, it is considered a positive requirement.
      // Accepts strings starting with '+' or otherwise not starting with '-' prefix

      std::vector<alf::types::TokenBase> pos_group;
      if (arg[0] == '+') {
        // do nothing (why?.. I forgot)
        if (arg[0] == '+' and arg.size() > 1) { // if user input a string like "+hello", this strips off the +
          pos_group.emplace_back(alf::types::SubStr{ std::string{ argv[i] + 1 }, true });
        }
        while (i + 1 < argc and strncmp(argv[i + 1], "+", 1) != 0 and strncmp(argv[i + 1], "-", 1) != 0) {
          pos_group.emplace_back(alf::types::SubStr{ std::string{ argv[i + 1] }, true });
          ++i;
        }
        add_or_pack(tokens, std::move(pos_group));
        continue;
      }
      if (arg[0] == '-') {
        std::vector<alf::types::TokenBase> neg_group;
        if (arg.size() > 1) { // if the user input a string like "-hello", this will strip off the -
          neg_group.emplace_back(alf::types::SubStr{ std::string{ argv[i] + 1 }, false });
        }
        while (i + 1 < argc and strncmp(argv[i + 1], "+", 1) != 0 and strncmp(argv[i + 1], "-", 1) != 0) {
          neg_group.emplace_back(alf::types::SubStr{ std::string{ argv[i + 1] }, false });
          ++i;
        }
        add_or_pack(tokens, std::move(neg_group));
        continue;
      }
      tokens.emplace_back(alf::types::SubStr{ arg, true});
    }


    return
      {
        std::move(infile), std::move(outfile), std::move(tokens)
      };
  }
} // end namespace alf
