#pragma once
#include <string>
#include <vector>
#include "Diagnostics.hpp"
#include "SymbolRegistry.hpp"
#include "Token.hpp"
 
std::vector<Token> tokenize(const std::string& source, SymbolRegistry& registry,
                            std::vector<Diagnostic>& diagnostics);
 