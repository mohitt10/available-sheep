#include "Frontend.hpp"
#include "Interpreter.hpp"
#include "Parser.hpp"
#include <cassert>
#include <climits>
#include <cstdio>

static FrontendResult compile(const std::string& src) {
    SimpleSymbolRegistry registry;
    return compileSource(src, registry);
}

static std::vector<std::int32_t> outputOf(const std::string& src, std::vector<std::int32_t> input = {}) {
    FrontendResult r = compile(src);
    assert(r.success);
    RunResult run_ = run(r.program, input);
    assert(run_.status == RunStatus::Ok);
    return run_.output;
}

static void testSampleProgram() {
    FrontendResult r = compile(
        "int a = 2 + 3;\n"
        "int b = a * input();\n"
        "print(b / a);\n");
    assert(r.success && r.diagnostics.empty());
    std::string expected =
        "0: t0 = 2 + 3\n"
        "1: a = t0\n"
        "2: t1 = input()\n"
        "3: t2 = a * t1\n"
        "4: b = t2\n"
        "5: t3 = b / a\n"
        "6: print t3\n";
    assert(toString(r.program) == expected);
    assert(validate(r.program).empty());
    assert(r.program.code[0].line == 1);
    assert(r.program.code[2].line == 2);
    assert(r.program.code[6].line == 3);

    RunResult rr = run(r.program, {4});
    assert(rr.output.size() == 1 && rr.output[0] == 4);
}

static void testDeterministic() {
    std::string src = "int x = 4; int y = x * 2; int z = x * 2; print(y + z);";
    assert(toString(compile(src).program) == toString(compile(src).program));
}

static void testPrecedence() {
    assert(outputOf("print(1 + 2 * 3);")[0] == 7);
    assert(outputOf("print((1 + 2) * 3);")[0] == 9);
    assert(outputOf("print(10 - 3 - 2);")[0] == 5);
    assert(outputOf("print(100 / 10 / 5);")[0] == 2);
    assert(outputOf("print(-2 * 3);")[0] == -6);
    assert(outputOf("print(1 < 2 == 1);")[0] == 1);
    assert(outputOf("print(!0);")[0] == 1);
    assert(outputOf("print(!5);")[0] == 0);
    assert(outputOf("print(7 % 4);")[0] == 3);
    assert(outputOf("print(--3);")[0] == 3);
    assert(outputOf("print(2147483647 + 1);")[0] == INT_MIN);
}

static void testReassignment() {
    FrontendResult r = compile("int x = 1;\nx = x + 1;\nprint(x);\n");
    assert(r.success);
    std::string expected =
        "0: x = 1\n"
        "1: t0 = x + 1\n"
        "2: x = t0\n"
        "3: print x\n";
    assert(toString(r.program) == expected);
    assert(run(r.program, {}).output[0] == 2);
}

static void testInputOrder() {
    auto out = outputOf("print(input() - input());", {10, 3});
    assert(out[0] == 7);
}

static void testComments() {
    auto out = outputOf("// hello\nint x = 1; /* mid */ print(x); // end");
    assert(out.size() == 1 && out[0] == 1);
}

static void testLexicalErrors() {
    FrontendResult a = compile("int x = 5 $ 3;");
    assert(!a.success && a.diagnostics.size() == 1);
    assert(a.diagnostics[0].stage == DiagStage::Lexical);
    assert(a.diagnostics[0].line == 1 && a.diagnostics[0].column == 11);
    assert(a.program.code.empty());

    FrontendResult b = compile("print(2147483648);");
    assert(!b.success && b.diagnostics[0].stage == DiagStage::Lexical);

    FrontendResult c = compile("int x = 1;\n/* never closed");
    assert(!c.success && c.diagnostics[0].stage == DiagStage::Lexical);
    assert(c.diagnostics[0].line == 2 && c.diagnostics[0].column == 1);

    FrontendResult d = compile("int x = 1 && 2;");
    assert(!d.success && d.diagnostics[0].stage == DiagStage::Lexical);

    FrontendResult e = compile("if (1) { }");
    assert(!e.success && e.diagnostics[0].stage == DiagStage::Lexical);
}

static void testSyntaxErrors() {
    FrontendResult a = compile("int x = 5");
    assert(!a.success && a.diagnostics.size() == 1);
    assert(a.diagnostics[0].stage == DiagStage::Syntax);

    FrontendResult b = compile("int x;");
    assert(!b.success && b.diagnostics[0].stage == DiagStage::Syntax);

    FrontendResult c = compile("while (1) print(1);");
    assert(!c.success && c.diagnostics[0].stage == DiagStage::Syntax);

    FrontendResult d = compile("print 1;");
    assert(!d.success && d.diagnostics[0].stage == DiagStage::Syntax);

    FrontendResult e = compile("print((1 + 2);");
    assert(!e.success && e.diagnostics[0].stage == DiagStage::Syntax);

    FrontendResult f = compile("int = 1;\nint y = ;\nprint(y);\n");
    assert(!f.success && f.diagnostics.size() == 2);
    assert(f.diagnostics[0].line == 1 && f.diagnostics[1].line == 2);

    FrontendResult g = compile("");
    assert(g.success && g.program.code.empty());

    FrontendResult h = compile("print(" + std::string(1000, '(') + "1" + std::string(1000, ')') + ");");
    assert(!h.success && h.diagnostics[0].stage == DiagStage::Syntax);

    FrontendResult i = compile(std::string("print(") + std::string(1000, '-') + "1);");
    assert(!i.success && i.diagnostics[0].stage == DiagStage::Syntax);
}

static void testSemanticErrors() {
    FrontendResult a = compile("print(x);");
    assert(!a.success && a.diagnostics[0].stage == DiagStage::Semantic);
    assert(a.diagnostics[0].message.find("undeclared") != std::string::npos);

    FrontendResult b = compile("int x = 1;\nint x = 2;");
    assert(!b.success && b.diagnostics.size() == 1);
    assert(b.diagnostics[0].line == 2);

    FrontendResult c = compile("int x = x;");
    assert(!c.success && c.diagnostics[0].stage == DiagStage::Semantic);

    FrontendResult d = compile("y = 1;");
    assert(!d.success && d.diagnostics[0].stage == DiagStage::Semantic);
}

static void testRegistry() {
    SimpleSymbolRegistry registry;
    FrontendResult r = compileSource("int abc = 1; int Abc = 2; abc = 3;", registry);
    assert(r.success);
    assert(registry.size() == 2);
    assert(r.program.symbols.size() == 2);
    assert(r.program.symbols[0].name == "abc" && r.program.symbols[1].name == "Abc");
    assert(r.program.symbols[0].id != r.program.symbols[1].id);
}

static void testSourceLocationsAndTrie() {
    SimpleSymbolRegistry registry;
    available_sheep::IdentifierTrie trie;
    FrontendResult result =
        compileSource("int item = 1 + 2;\nprint(item);", registry, &trie);
    assert(result.success);
    assert(result.program.code[0].line == 1);
    assert(result.program.code[0].column == 14);
    assert(result.program.code[1].column == 1);
    assert(trie.contains("item"));
    const std::vector<SymbolId> ids = trie.findExact("item");
    assert(ids.size() == 1 && ids[0] == result.program.symbols[0].id);
    assert(!result.identifierIndex.empty());

    FrontendResult localIndex = compile("int local = 1;");
    assert(localIndex.success);
    assert(localIndex.identifierIndex.size() == 1);
    assert(localIndex.identifierIndex.front().spelling == "local");

    std::vector<Token> missingEnd;
    std::vector<Diagnostic> diagnostics;
    Ast invalid = parse(missingEnd, diagnostics);
    assert(invalid.statements.empty());
    assert(diagnostics.size() == 1);
    assert(diagnostics.front().stage == DiagStage::Syntax);
}

int main() {
    testSampleProgram();
    testDeterministic();
    testPrecedence();
    testReassignment();
    testInputOrder();
    testComments();
    testLexicalErrors();
    testSyntaxErrors();
    testSemanticErrors();
    testRegistry();
    testSourceLocationsAndTrie();
    std::puts("frontend tests passed");
}