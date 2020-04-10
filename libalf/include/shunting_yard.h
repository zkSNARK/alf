//
// Created by Christopher Goebel on 3/24/20.
//

#pragma once

#include "alf_types.h"

#include <vector>
#include <queue>

namespace alf
{
  [[nodiscard]] auto shunting_yard(std::vector<alf::types::TokenBase> v) -> std::vector<alf::types::TokenBase>;
}