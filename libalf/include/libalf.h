#pragma once

#include <string>
#include <vector>
#include <tuple>
#include <optional>

using FilterPack = std::vector<std::vector<std::string>>;
using FileNameOpt = std::optional<std::string>;
using OptStr = std::optional<std::string>;

struct ArgPack
{
  explicit ArgPack(std::tuple<OptStr, OptStr, FilterPack, FilterPack> t)
    : infile(std::get<0>(t)),
      outfile(std::get<1>(t)),
      pos(std::get<2>(t)),
      neg(std::get<3>(t))
  {}

  ~ArgPack() = default;
  ArgPack(ArgPack const&) = default;
  ArgPack &operator=(ArgPack const&) = default;
  ArgPack(ArgPack&&) = default;
  ArgPack& operator=(ArgPack&&) = default;

  FileNameOpt infile;
  FileNameOpt outfile;
  FilterPack pos;
  FilterPack neg;
};

auto parse_for_shunting(std::string_view const& s) -> std::vector<std::string>;

auto parse_args(int argc, const char **argv) -> std::tuple<FileNameOpt, FileNameOpt, FilterPack, FilterPack>;

auto passes_filters(FilterPack const &fp_pos, FilterPack const &fp_neg, std::string const &line) -> bool;

auto apply_filters(ArgPack p) -> void;