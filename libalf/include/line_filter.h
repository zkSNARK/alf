//
// Created by Christopher Goebel on 4/5/20.
//

#pragma once

#include "alf_types.h"
#include "parser.h"

#include <queue>
#include <stdexcept>
#include <stack>

namespace alf
{
  auto passes_filters(std::vector<types::TokenBase> const& filters, std::string const& line) -> bool;

  auto apply_filters(parser::ArgPack p) -> void;

  auto handle_operator(std::stack<bool>& stk, types::TYPE_TOKEN tt) -> void;
}