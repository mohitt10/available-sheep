#pragma once
#include <vector>
#include "Ast.hpp"
#include "Diagnostics.hpp"
 
void analyze(const Ast& ast, std::vector<Diagnostic>& diagnostics);
 