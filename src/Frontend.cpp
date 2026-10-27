#include "Frontend.hpp"
#include "Irgenerator.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "Semantic.hpp"

FrontendResult compileSource(const std::string& source, SymbolRegistry& registry,
                             available_sheep::IdentifierTrie* trie) {
    FrontendResult result;

    available_sheep::IdentifierTrie localTrie;
    available_sheep::IdentifierTrie& activeTrie = trie ? *trie : localTrie;
    std::vector<Token> tokens =
        tokenize(source, registry, result.diagnostics, &activeTrie);
    result.identifierIndex = activeTrie.snapshot();
    if (!result.diagnostics.empty()) return result;

    Ast ast = parse(tokens, result.diagnostics);
    if (!result.diagnostics.empty()) return result;

    analyze(ast, result.diagnostics);
    if (!result.diagnostics.empty()) return result;

    Program program = generateIr(ast);
    std::vector<std::string> problems = validate(program);
    if (!problems.empty()) {
        Diagnostic d;
        d.stage = DiagStage::Semantic;
        d.message = "internal error: generated IR is invalid: " + problems[0];
        result.diagnostics.push_back(d);
        return result;
    }

    result.program = program;
    result.success = true;
    return result;
}