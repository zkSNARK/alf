#pragma once

#include "alf_types.h"

#include <string>
#include <utility>
#include <vector>
#include <tuple>
#include <optional>

namespace alf
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

  std::vector<alf::types::TokenBase> fill_in_missing_AND_symbols(std::vector<alf::types::TokenBase>& v);

  auto parse_algebraic(std::string_view const& sv) -> std::vector<alf::types::TokenBase>;

  auto parse_arguments(int argc, const char** argv) -> ArgPack;

//  auto passes_filters(std::vector<alf::types::TokenBase> tokens, const& fp_pos, std::vector<alf::types::TokenBase> tokens, const& fp_neg, std::string const& line) -> bool;
//
//  auto apply_filters(ArgPack p) -> void;
}