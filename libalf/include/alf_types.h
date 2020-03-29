//
// alf_types.h
//
// Created by Christopher Goebel on 3/28/20.
//



#pragma once

#include <string>
#include <utility>

namespace alf::types {
  enum class TYPE_TOKEN
  {
    SUBSTR,

    BRACKET_OPEN_CURLY,
    BRACKET_CLOSE_CURLY,
    BRACKET_OPEN_PAREN,
    BRACKET_CLOSE_PAREN,
    BRACKET_OPEN_SQUARE,
    BRACKET_CLOSE_SQUARE,

    OPERATOR_AND,
    OPERATOR_OR,
    OPERATOR_XOR
  };

  struct TokenBase
  {
    std::string value;
    bool required;
    TYPE_TOKEN type;

    TokenBase(TYPE_TOKEN type, std::string value, bool require)
        : type(type),
          value(std::move(value)),
          required(require) { }

    virtual ~TokenBase() = default;

    auto operator==(const TokenBase& other) const {
      return type == other.type && value == other.value && required == other.required;
    }
  };

  struct SubStr
      : public TokenBase
  {
    SubStr(std::string value, bool require)
        : TokenBase(TYPE_TOKEN::SUBSTR, std::move(value), require) { }

    explicit SubStr(std::string_view value, bool require)
        : TokenBase(TYPE_TOKEN::SUBSTR, std::string(value), require) { }
  };

  struct Operator
      : public TokenBase
  {
    Operator(TYPE_TOKEN type, std::string value, bool require)
        : TokenBase(type, std::move(value), require) { }
  };

  namespace operators {
    struct BinaryOperator
        : public Operator
    {
      explicit BinaryOperator(TYPE_TOKEN type, std::string value)
          : Operator(type, std::move(value), true) { }
    };

    struct AND
        : public BinaryOperator
    {
      AND()
          : BinaryOperator(TYPE_TOKEN::OPERATOR_AND, "&") { }
    };

    struct OR
        : public BinaryOperator
    {
      OR()
          : BinaryOperator(TYPE_TOKEN::OPERATOR_OR, "|") { }
    };

    struct XOR
        : public BinaryOperator
    {
      XOR()
          : BinaryOperator(TYPE_TOKEN::OPERATOR_XOR, "^") { }
    };
  } // end namespace operators

  namespace brackets {
    struct Bracket
        : public TokenBase
    {
      explicit Bracket(TYPE_TOKEN type, std::string value)
          : TokenBase(type, std::move(value), true) { }
    };

    struct BracketOpen
        : public Bracket
    {
      explicit BracketOpen(TYPE_TOKEN type, std::string value)
          : Bracket(type, std::move(value)) { }
    };

    struct OpenParen
        : public BracketOpen
    {
      OpenParen()
          : BracketOpen(TYPE_TOKEN::BRACKET_OPEN_PAREN, "(") { }
    };

    struct OpenSquareBracket
        : public BracketOpen
    {
      OpenSquareBracket()
          : BracketOpen(TYPE_TOKEN::BRACKET_OPEN_SQUARE, "[") { }
    };

    struct OpenCurlyBracket
        : public BracketOpen
    {
      OpenCurlyBracket()
          : BracketOpen(TYPE_TOKEN::BRACKET_OPEN_CURLY, "{") { }
    };

    struct BracketClose
        : public Bracket
    {
      explicit BracketClose(TYPE_TOKEN type, std::string value)
          : Bracket(type, std::move(value)) { }
    };

    struct CloseParen
        : public BracketClose
    {
      CloseParen()
          : BracketClose(TYPE_TOKEN::BRACKET_CLOSE_PAREN, ")") { }
    };

    struct CloseSquareBracket
        : public BracketClose
    {
      CloseSquareBracket()
          : BracketClose(TYPE_TOKEN::BRACKET_CLOSE_SQUARE, "]") { }
    };

    struct CloseCurlyBracket
        : public BracketClose
    {
      CloseCurlyBracket()
          : BracketClose(TYPE_TOKEN::BRACKET_CLOSE_CURLY, "}") { }
    };
  } // end namespace brackets
} // end namespace alf
