//
// Created by Christopher Goebel on 3/24/20.
//

#pragma once

#include <vector>
#include <queue>
#include <string>

auto is_opening_paren(char c) -> bool;

auto companion_parenthesis(char c) -> char;

auto shunting_yard(std::vector<alf::types::TokenBase> v) -> std::queue<alf::types::TokenBase>;