#pragma once
#include <string>
#include <vector>
#include "Diagnostics.hpp"
#include "Ir.hpp"
#include "SymbolRegistry.hpp"
 
struct FrontendResult {
    bool success = false;
    std::vector<Diagnostic> diagnostics;
    Program program;
};
 
FrontendResult compileSource(const std::string& source, SymbolRegistry& registry);
 