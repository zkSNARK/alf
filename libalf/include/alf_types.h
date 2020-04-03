//
// alf_types.h
//
// Created by Christopher Goebel on 3/28/20.
//


#pragma once

#include <string>
#include <utility>
#include <vector>


namespace alf::types
{

  /**
   * A token enum containing all the possible token types that the parser can create.
   */
  enum class TYPE_TOKEN : char
  {
    SUBSTR,

    OPERATOR_OR,
    OPERATOR_XOR,
    OPERATOR_AND,

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
    }
  }


  /**
   * Tokens are not polymorphic, but there is a hierarchy.
   * Sub categories are
   *    - substrings
   *    - binary operators (and, or, xor)
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

    TokenBase(TYPE_TOKEN type, std::string value, bool require)
      : type(type),
        value(std::move(value)),
        required(require) { }

    auto operator==(const TokenBase& other) const
    {
      return type == other.type && value == other.value && required == other.required;
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
  };

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
           or t == TYPE_TOKEN::OPERATOR_XOR;
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
