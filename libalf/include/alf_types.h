//
// alf_types.h
//
// Created by Christopher Goebel on 3/28/20.
//


#pragma once

#include <memory>
#include <regex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <optional>


namespace alf::types
{
  /**
   * A token enum containing all the possible token types that the parser can create.
   */
  enum class TYPE_TOKEN : char
  {
    SUBSTR,
    REGEX,

    OPERATOR_OR,
    OPERATOR_XOR,
    OPERATOR_AND,
    OPERATOR_NOT,   // unary prefix: negates the group that follows, as in -( ... ) or !( ... )

    BRACKET_OPEN_CURLY,
    BRACKET_CLOSE_CURLY,
    BRACKET_OPEN_PAREN,
    BRACKET_CLOSE_PAREN,
    BRACKET_OPEN_SQUARE,
    BRACKET_CLOSE_SQUARE
  };


  inline auto type_token_to_string(TYPE_TOKEN tt) -> std::string
  {
    switch (tt) {
      case TYPE_TOKEN::SUBSTR :
        return "SUBSTR";
      case TYPE_TOKEN::REGEX :
        return "REGEX";
      case TYPE_TOKEN::BRACKET_OPEN_CURLY:
        return "BRACKET_OPEN_CURLY";
      case TYPE_TOKEN::BRACKET_CLOSE_CURLY:
        return "BRACKET_CLOSE_CURLY";
      case TYPE_TOKEN::BRACKET_OPEN_PAREN:
        return "BRACKET_OPEN_PAREN";
      case TYPE_TOKEN::BRACKET_CLOSE_PAREN:
        return "BRACKET_CLOSE_PAREN";
      case TYPE_TOKEN::BRACKET_OPEN_SQUARE:
        return "BRACKET_OPEN_SQUARE";
      case TYPE_TOKEN::BRACKET_CLOSE_SQUARE:
        return "BRACKET_CLOSE_SQUARE";
      case TYPE_TOKEN::OPERATOR_AND:
        return "OPERATOR_AND";
      case TYPE_TOKEN::OPERATOR_OR:
        return "OPERATOR_OR";
      case TYPE_TOKEN::OPERATOR_XOR:
        return "OPERATOR_XOR";
      case TYPE_TOKEN::OPERATOR_NOT:
        return "OPERATOR_NOT";
    }
    return "UNKNOWN";
  }


  /**
   * Tokens are not polymorphic, but there is a hierarchy.
   * Sub categories are
   *    - substrings and regular expressions (the operands)
   *    - binary operators (and, or, xor) and the unary not
   *    - brackets (parenthesis, square, curly)
   *
   *  NOTE: if the bool required is false, then that implies
   *        that the user specifically instructed us to not
   *        allow a token... it is a "negative requirement"
   *
   *        E.G. give me all the lines that do not contain
   *        the substr "hello".
   *
   *        Most tokens will be positive requirements, but
   *        all tokens have this field.
   */
  struct TokenBase
  {
    std::string value;
    bool required;
    TYPE_TOKEN type;
    std::shared_ptr<std::regex const> regex;  // REGEX only: compiled once, shared by copies
    bool nocase{ false };  // SUBSTR, i'...': ASCII letters match either case
    bool word{ false };    // SUBSTR, w'...': only where it isn't part of a longer word

    TokenBase(TYPE_TOKEN type, std::string value, bool require)
      : value(std::move(value)),
        required(require),
        type(type) { }

    auto operator==(const TokenBase& other) const
    {
      return type == other.type && value == other.value && required == other.required && nocase == other.nocase &&
             word == other.word;
    }
  };


  /**
   * SubStr is the most common token type.  Represents something that a user
   * will be requiring or requiring not to be there.
   */
  struct SubStr : public TokenBase
  {
    SubStr(std::string value, bool require) : TokenBase(TYPE_TOKEN::SUBSTR, std::move(value), require) { }

    explicit SubStr(std::string_view value, bool require)
      : TokenBase(TYPE_TOKEN::SUBSTR, std::string(value), require) { }

    /**
     * i'text' (nocase): ASCII letters match in either case, as grep -i does with LC_ALL=C.
     * w'text' (word): only occurrences with no word byte ([A-Za-z0-9_]) right before or after
     * them, as grep -w.  iw'text' is both.
     */
    SubStr(std::string value, bool require, bool nocase_, bool word_)
      : TokenBase(TYPE_TOKEN::SUBSTR, std::move(value), require)
    {
      nocase = nocase_;
      word = word_;
    }
  };

  /**
   * A regular expression written r'pattern' or r"pattern".  A line matches if the pattern is
   * found anywhere in it (std::regex_search, ECMAScript grammar).
   */
  struct Regex : public TokenBase
  {
    Regex(std::string pattern, bool require) : TokenBase(TYPE_TOKEN::REGEX, std::move(pattern), require)
    {
      try {
        regex = std::make_shared<std::regex const>(value, std::regex::ECMAScript | std::regex::optimize);
      } catch (std::regex_error const& e) {
        throw std::runtime_error("invalid regex '" + value + "': " + e.what());
      }
    }
  };

  inline auto is_operand(TYPE_TOKEN const& t) -> bool
  {
    return t == TYPE_TOKEN::SUBSTR or t == TYPE_TOKEN::REGEX;
  }

  namespace operators
  {
    struct Operator : public TokenBase
    {
      Operator(TYPE_TOKEN type, std::string value, bool require) : TokenBase(type, std::move(value), require) { }
    };

    struct BinaryOperator : public Operator
    {
    protected:
      explicit BinaryOperator(TYPE_TOKEN type, std::string value) : Operator(type, std::move(value), true) { }
    };

    struct AND : public BinaryOperator
    {
      AND() : BinaryOperator(TYPE_TOKEN::OPERATOR_AND, "&") { }
    };

    struct OR : public BinaryOperator
    {
      OR() : BinaryOperator(TYPE_TOKEN::OPERATOR_OR, "|") { }
    };

    struct XOR : public BinaryOperator
    {
      XOR() : BinaryOperator(TYPE_TOKEN::OPERATOR_XOR, "^") { }
    };

    /**
     * Negates a bracketed group.  A negated substring doesn't need it: '-a' and '!a' are stored
     * as SubStr{"a", required = false}.
     */
    struct NOT : public Operator
    {
      NOT() : Operator(TYPE_TOKEN::OPERATOR_NOT, "!", true) { }
    };
  } // end namespace operators

  namespace brackets
  {
    struct Bracket : public TokenBase
    {
      explicit Bracket(TYPE_TOKEN type, std::string value) : TokenBase(type, std::move(value), true) { }
    };

    struct BracketOpen : public Bracket
    {
    protected:
      explicit BracketOpen(TYPE_TOKEN type, std::string value) : Bracket(type, std::move(value)) { }
    };

    struct OpenParen : public BracketOpen
    {
      OpenParen() : BracketOpen(TYPE_TOKEN::BRACKET_OPEN_PAREN, "(") { }
    };

    struct OpenSquareBracket : public BracketOpen
    {
      OpenSquareBracket() : BracketOpen(TYPE_TOKEN::BRACKET_OPEN_SQUARE, "[") { }
    };

    struct OpenCurlyBracket : public BracketOpen
    {
      OpenCurlyBracket() : BracketOpen(TYPE_TOKEN::BRACKET_OPEN_CURLY, "{") { }
    };

    struct BracketClose : public Bracket
    {
      explicit BracketClose(TYPE_TOKEN type, std::string value) : Bracket(type, std::move(value)) { }
    };

    struct CloseParen : public BracketClose
    {
      CloseParen() : BracketClose(TYPE_TOKEN::BRACKET_CLOSE_PAREN, ")") { }
    };

    struct CloseSquareBracket : public BracketClose
    {
      CloseSquareBracket() : BracketClose(TYPE_TOKEN::BRACKET_CLOSE_SQUARE, "]") { }
    };

    struct CloseCurlyBracket : public BracketClose
    {
      CloseCurlyBracket() : BracketClose(TYPE_TOKEN::BRACKET_CLOSE_CURLY, "}") { }
    };
  } // end namespace brackets

  inline auto is_operator(alf::types::TYPE_TOKEN const& t) -> bool
  {
    return t == TYPE_TOKEN::OPERATOR_AND
           or t == TYPE_TOKEN::OPERATOR_OR
           or t == TYPE_TOKEN::OPERATOR_XOR
           or t == TYPE_TOKEN::OPERATOR_NOT;
  }

  inline auto is_unary_operator(alf::types::TYPE_TOKEN const& t) -> bool
  {
    return t == TYPE_TOKEN::OPERATOR_NOT;
  }

  inline auto is_opening_bracket(alf::types::TYPE_TOKEN const& t) -> bool
  {
    return t == TYPE_TOKEN::BRACKET_OPEN_CURLY
           or t == TYPE_TOKEN::BRACKET_OPEN_PAREN
           or t == TYPE_TOKEN::BRACKET_OPEN_SQUARE;
  }

  inline auto is_closing_bracket(alf::types::TYPE_TOKEN const& t) -> bool
  {
    return t == TYPE_TOKEN::BRACKET_CLOSE_CURLY
           or t == TYPE_TOKEN::BRACKET_CLOSE_SQUARE
           or t == TYPE_TOKEN::BRACKET_CLOSE_PAREN;
  }
} // end namespace alf
