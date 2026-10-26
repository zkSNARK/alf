//
// Created by Christopher Goebel on 3/21/20.
//

#include "parser.h"

#include <algorithm>
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


  /**
   * A '-' or '!' must be followed by a string or a bracketed group, never by an operator or a
   * closing bracket ("a & -" or "-)" mean nothing).
   */
  void require_operand_after_negation(bool is_pos_req, char c)
  {
    if (!is_pos_req) {
      throw std::runtime_error(std::string("'-' or '!' must be followed by a string or a group, not '") + c + "'");
    }
  }


  /**
   * A negated opening bracket negates the whole group: emit a unary NOT in front of it.
   */
  template <typename Bracket>
  void open_group(bool& is_pos_req, std::vector<alf::types::TokenBase>& tokens)
  {
    if (!is_pos_req) {
      tokens.emplace_back(alf::types::operators::NOT{});
    }
    tokens.emplace_back(Bracket{});
    is_pos_req = true;
  }


  auto make_token(std::string_view token, bool& is_pos_req, std::vector<alf::types::TokenBase>& tokens)
  {
    switch (token[0]) {
      case '|': {
        require_operand_after_negation(is_pos_req, token[0]);
        tokens.emplace_back(alf::types::operators::OR{});
        break;
      }
      case '&': {
        require_operand_after_negation(is_pos_req, token[0]);
        tokens.emplace_back(alf::types::operators::AND{});
        break;
      }
      case '^': {
        require_operand_after_negation(is_pos_req, token[0]);
        tokens.emplace_back(alf::types::operators::XOR{});
        break;
      }
      case '+': {
        break;
      }
      case '!':
      case '-': {
        is_pos_req = !is_pos_req;  // a second negation cancels the first
        break;
      }
      case '(': {
        open_group<alf::types::brackets::OpenParen>(is_pos_req, tokens);
        break;
      }
      case '[': {
        open_group<alf::types::brackets::OpenSquareBracket>(is_pos_req, tokens);
        break;
      }
      case '{': {
        open_group<alf::types::brackets::OpenCurlyBracket>(is_pos_req, tokens);
        break;
      }
      case ')': {
        require_operand_after_negation(is_pos_req, token[0]);
        tokens.emplace_back(alf::types::brackets::CloseParen{});
        break;
      }
      case ']': {
        require_operand_after_negation(is_pos_req, token[0]);
        tokens.emplace_back(alf::types::brackets::CloseSquareBracket{});
        break;
      }
      case '}': {
        require_operand_after_negation(is_pos_req, token[0]);
        tokens.emplace_back(alf::types::brackets::CloseCurlyBracket{});
        break;
      }
      default:
        tokens.emplace_back(alf::types::SubStr{ token, is_pos_req });
        is_pos_req = true;
        break;
    }
  }


  /**
   * `second` points at an opening quote.  Reads up to the matching quote and emits a SubStr, or
   * a Regex when the quote was prefixed with r (as in r'^\\d+').  `prefix` is what was written
   * right against the quote: "", "r", or i / w / iw / wi for a case-insensitive and/or whole-word
   * string.  There are no escapes: to use one kind of quote inside a string or pattern, delimit it
   * with the other kind.
   */
  auto handle_quote(char const*& first, char const*& second, char const*& last,
                    std::vector<alf::types::TokenBase>& tokens, bool& is_pos_req, std::string_view prefix)
  {
    char const quote = *second;
    first = second + 1;
    second = std::find(first, last, quote);
    if (second == last) {
      throw std::runtime_error("imperfect quotes");
    }

    std::string value(first, second - first);
    if (prefix == "r") {
      tokens.emplace_back(alf::types::Regex{ std::move(value), is_pos_req });
    } else {
      bool const nocase = prefix.find('i') != std::string_view::npos, word = prefix.find('w') != std::string_view::npos;
      tokens.emplace_back(alf::types::SubStr{ std::move(value), is_pos_req, nocase, word });
    }
    is_pos_req = true;
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
  auto fill_in_missing_AND_symbols(std::vector<types::TokenBase>& v) -> std::vector<types::TokenBase>
  {
    using alf::types::TYPE_TOKEN;

    std::vector<types::TokenBase> tokens;
    tokens.reserve(v.size() * 2);

    TYPE_TOKEN previous_type = TYPE_TOKEN::OPERATOR_AND;
    for (types::TokenBase& t : v) {
      TYPE_TOKEN cur_type = t.type;
      if (is_operand(cur_type) and
          (is_operand(previous_type) or is_closing_bracket(previous_type))) {
        tokens.emplace_back(alf::types::operators::AND{});
      } else if ((is_opening_bracket(cur_type) or is_unary_operator(cur_type)) and
                 (is_operand(previous_type) or is_closing_bracket(previous_type))) {
        tokens.emplace_back(alf::types::operators::AND{});
      }
      tokens.emplace_back(std::move(t));
      previous_type = cur_type;
    }
    return tokens;
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
  auto parse_algebraic(std::string_view const& s) -> std::vector<types::TokenBase>
  {
    static std::string_view const delims = " '\"&|{([])}!^-+";
    std::stack<char> paren_stk;
    std::vector<alf::types::TokenBase> tokens;
    tokens.reserve(s.size() / 2);

    bool is_positive_req = true;

    for (auto first = s.data(), second = s.data(), last = first + s.size();
         second != last && first != last; first = second + 1) {
      second = std::find_first_of(first, last, std::cbegin(delims), std::cend(delims));

      // letters written right against a quote are a prefix: r'pattern' is a regex, i'text' a
      // case-insensitive string, w'text' a whole word, iw'text' (or wi) both
      std::string_view const before(first, static_cast<std::size_t>(second - first));
      bool const prefixed = second != last and is_quote(*second) and
                            (before == "r" or before == "i" or before == "w" or before == "iw" or before == "wi");
      if (first != second and !prefixed) {
        make_token(std::string_view(first, second - first), is_positive_req, tokens);
      }
      if (second == last) {
        break;
      }
      if (is_quote(*second)) {
        handle_quote(first, second, last, tokens, is_positive_req, prefixed ? before : std::string_view{});
      } else if (*second != ' ') {
        make_token(std::string_view{ second, 1 }, is_positive_req, tokens);
      }
    }
    if (!is_positive_req) {
      throw std::runtime_error("expression ends with '-' or '!'");
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
    unsigned threads = 0;

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
      if (arg == "--threads") {
        if (i + 1 >= argc or std::strspn(argv[i + 1], "0123456789") != std::strlen(argv[i + 1]) or !*argv[i + 1]) {
          throw std::runtime_error("--threads must be followed by a number");
        }
        threads = static_cast<unsigned>(std::stoul(argv[i + 1]));
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
      tokens.emplace_back(alf::types::SubStr{ arg, true });
    }

    ArgPack pack{ std::move(infile), std::move(outfile), std::move(tokens) };
    pack.threads = threads;
    return pack;
  }
} // end namespace alf
