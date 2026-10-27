#pragma once
#include <vector>
#include "Ast.hpp"
#include "Diagnostics.hpp"
#include "Token.hpp"
 
Ast parse(const std::vector<Token>& tokens, std::vector<Diagnostic>& diagnostics);
 