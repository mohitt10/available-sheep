#pragma once
#include <string>
#include <vector>
#include "Diagnostics.hpp"
#include "Ir.hpp"
#include "SymbolRegistry.hpp"
#include "identifier_trie.hpp"
 
struct FrontendResult {
    bool success = false;
    std::vector<Diagnostic> diagnostics;
    Program program;
    std::vector<available_sheep::TrieEntry> identifierIndex;
};
 
FrontendResult compileSource(const std::string& source, SymbolRegistry& registry,
                             available_sheep::IdentifierTrie* trie = nullptr);
 