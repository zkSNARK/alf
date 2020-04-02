//
// Created by Christopher Goebel on 3/24/20.
//

#pragma once

#include <vector>
#include <queue>
#include <string>

auto is_opening_paren(char c) -> bool;

auto is_closing_paren(char c) -> bool;

auto companion_parenthesis(char c) -> char;

auto is_operator(std::string const &s) -> bool;

auto is_parenthesis(std::string const &s) -> bool;

auto precedence(char c) -> int;

auto shunting_yard(std::vector<std::string> v) -> std::queue<std::string>;