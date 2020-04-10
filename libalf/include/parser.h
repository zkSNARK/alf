#pragma once

#include "alf_types.h"

#include <string>
#include <utility>
#include <vector>
#include <tuple>

namespace alf::parser
{
  using FileNameOpt = std::optional<std::string>;
  using OptStr = std::optional<std::string>;

  struct ArgPack
  {
    ArgPack(OptStr infile, OptStr outfile, std::vector<alf::types::TokenBase> tokens)
      : infile(std::move(infile)),
        outfile(std::move(outfile)),
        tokens(std::move(tokens))
    { }

    ~ArgPack() = default;
    ArgPack(ArgPack const&) = default;
    ArgPack& operator=(ArgPack const&) = default;
    ArgPack(ArgPack&&) = default;
    ArgPack& operator=(ArgPack&&) = default;

    FileNameOpt infile;
    FileNameOpt outfile;
    std::vector<alf::types::TokenBase> tokens;
  };

  [[nodiscard]] auto fill_in_missing_AND_symbols(std::vector<alf::types::TokenBase>& v) -> std::vector<alf::types::TokenBase>;

  [[nodiscard]] auto parse_algebraic(std::string_view const& sv) -> std::vector<alf::types::TokenBase>;

  [[nodiscard]] auto parse_arguments(int argc, const char** argv) -> ArgPack;
}