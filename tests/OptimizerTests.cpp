#include "Frontend.hpp"
#include "Interpreter.hpp"
#include "Optimizer.hpp"

#include <algorithm>
#include <cassert>
#include <climits>
#include <cstdio>

namespace {

FrontendResult compile(const std::string& source,
                       available_sheep::IdentifierTrie* trie = nullptr) {
    static SimpleSymbolRegistry registry;
    return compileSource(source, registry, trie);
}

void assertSameOutput(const Program& before, const Program& after,
                      const std::vector<std::int32_t>& input = {}) {
    const RunResult initial = run(before, input);
    const RunResult optimized = run(after, input);
    assert(initial.status == optimized.status);
    assert(initial.output == optimized.output);
}

void testFixedPointAndSemantics() {
    FrontendResult frontend =
        compile("int x = 2; int y = x + 3; print(y);");
    assert(frontend.success);
    OptimizationResult result = optimizeProgram(frontend.program);
    assert(result.outcome == RunOutcome::Optimized);
    assert(result.convergence == Convergence::Converged);
    assert(result.iterationsRun > 1);
    assert(result.before.instructions > result.after.instructions);
    assertSameOutput(result.initialIr, result.optimizedIr);
    assert(run(result.optimizedIr, {}).output == std::vector<std::int32_t>{5});
}

void testBindingUpdatesAndInputEffects() {
    FrontendResult reassignment =
        compile("int x = 2; print(x); x = 3; print(x);");
    assert(reassignment.success);
    OptimizationResult propagated = optimizeProgram(reassignment.program);
    assertSameOutput(reassignment.program, propagated.optimizedIr);
    assert(run(propagated.optimizedIr, {}).output ==
           (std::vector<std::int32_t>{2, 3}));

    FrontendResult input =
        compile("int x = input(); int y = input(); print(x + y);");
    assert(input.success);
    OptimizationResult inputResult = optimizeProgram(input.program);
    assertSameOutput(input.program, inputResult.optimizedIr, {4, 7});
    int inputOps = 0;
    for (const Instruction& instruction : inputResult.optimizedIr.code)
        if (instruction.op == Opcode::Input) ++inputOps;
    assert(inputOps == 2);
}

void testCseUsesValueVersionsAndPreservesTraps() {
    FrontendResult frontend = compile(
        "int x = input(); int a = 12 / x; int b = 12 / x; print(a + b);");
    assert(frontend.success);
    Program program = frontend.program;
    std::unique_ptr<OptimizationPass> cse =
        makeOptimizationPass(PassKind::CommonSubexpressionElimination);
    PassStats stats = cse->run(program);
    assert(!stats.changed);
    int divides = 0;
    for (const Instruction& instruction : program.code)
        if (instruction.op == Opcode::Div) ++divides;
    assert(divides == 2);
    assert(validate(program).empty());
    assertSameOutput(frontend.program, program, {3});
    assert(run(frontend.program, {0}).status == RunStatus::DivisionByZero);
    assert(run(program, {0}).status == RunStatus::DivisionByZero);

    FrontendResult rebound = compile(
        "int x = input(); int a = x + 1; x = input(); "
        "int b = x + 1; print(a + b);");
    assert(rebound.success);
    Program reboundProgram = rebound.program;
    PassStats reboundStats = cse->run(reboundProgram);
    assert(!reboundStats.changed);
    assertSameOutput(rebound.program, reboundProgram, {2, 8});
}

void testPureCseAndDce() {
    FrontendResult frontend = compile(
        "int x = input(); int a = x + 4; int b = x + 4; print(a + b);");
    assert(frontend.success);
    Program cseProgram = frontend.program;
    const int addsBefore = static_cast<int>(std::count_if(
        cseProgram.code.begin(), cseProgram.code.end(),
        [](const Instruction& instruction) { return instruction.op == Opcode::Add; }));
    std::unique_ptr<OptimizationPass> cse =
        makeOptimizationPass(PassKind::CommonSubexpressionElimination);
    PassStats cseStats = cse->run(cseProgram);
    const int addsAfter = static_cast<int>(std::count_if(
        cseProgram.code.begin(), cseProgram.code.end(),
        [](const Instruction& instruction) { return instruction.op == Opcode::Add; }));
    assert(cseStats.changed && addsAfter == addsBefore - 1);
    assert(validate(cseProgram).empty());
    assertSameOutput(frontend.program, cseProgram, {5});

    Program effects;
    effects.tempCount = 3;
    effects.code = {
        makeInstruction(Opcode::Input, Operand::temp(0)),
        makeInstruction(Opcode::Add, Operand::temp(1), Operand::constant(1),
                        Operand::constant(2)),
        makeInstruction(Opcode::Div, Operand::temp(2), Operand::constant(8),
                        Operand::constant(0)),
    };
    std::unique_ptr<OptimizationPass> dce =
        makeOptimizationPass(PassKind::DeadCodeElimination);
    PassStats dceStats = dce->run(effects);
    assert(dceStats.instructionsRemoved == 1);
    assert(effects.code.size() == 2);
    assert(effects.code[0].op == Opcode::Input);
    assert(effects.code[1].op == Opcode::Div);
    assert(validate(effects).empty());

    FrontendResult deadStores = compile("int x = input(); x = 9;");
    assert(deadStores.success);
    Program deadStoreProgram = deadStores.program;
    PassStats storesStats = dce->run(deadStoreProgram);
    assert(storesStats.changed);
    assert(deadStoreProgram.code.size() == 1);
    assert(deadStoreProgram.code.front().op == Opcode::Input);
    assert(validate(deadStoreProgram).empty());
}

void testLimitsAndSnapshots() {
    FrontendResult frontend = compile("int x = 2; int y = x + 3; print(y);");
    assert(frontend.success);
    PassOptions noIterations;
    noIterations.maxIterations = 0;
    OptimizationResult limited = optimizeProgram(frontend.program, noIterations);
    assert(limited.outcome == RunOutcome::Optimized);
    assert(limited.convergence == Convergence::IterationLimit);
    assert(limited.iterationsRun == 0);
    assert(limited.optimizedIr.code.size() == frontend.program.code.size());

    PassOptions noPasses;
    for (int i = 0; i < kPassCount; ++i) noPasses.enabled[i] = false;
    OptimizationResult unchanged = optimizeProgram(frontend.program, noPasses);
    assert(unchanged.convergence == Convergence::Converged);
    assert(unchanged.iterationsRun == 1);
    assert(!unchanged.iterations.front().changed);

    SimpleSymbolRegistry registry;
    available_sheep::IdentifierTrie trie;
    FrontendResult withTrie = compileSource(
        "int x = 2 + 3; print(x);", registry, &trie);
    assert(withTrie.success);
    OptimizationResult snapshots =
        optimizeProgram(withTrie.program, PassOptions{}, &trie);
    assert(!snapshots.dagNodes.empty());
    assert(!snapshots.avlNodes.empty());
    assert(!snapshots.trieEntries.empty());
    assert(snapshots.avlNodes.front().height > 0);
    for (const auto& node : snapshots.avlNodes)
        assert(node.id == node.info.dagNodeId);
    for (const auto& node : snapshots.dagNodes)
        assert(node.key.type == 1);
}

void testConstantOperationEdgesAndInvalidIr() {
    FrontendResult wrapped = compile("print(2147483647 + 1);");
    assert(wrapped.success);
    OptimizationResult folded = optimizeProgram(wrapped.program);
    assert(run(folded.optimizedIr, {}).output ==
           std::vector<std::int32_t>{INT_MIN});

    FrontendResult zeroDivision = compile("print(1 / 0);");
    assert(zeroDivision.success);
    OptimizationResult trapped = optimizeProgram(zeroDivision.program);
    assert(run(trapped.optimizedIr, {}).status == RunStatus::DivisionByZero);

    Program invalid;
    invalid.code.push_back(makeInstruction(
        Opcode::Print, Operand::none(), Operand::temp(0)));
    OptimizationResult rejected = optimizeProgram(invalid);
    assert(rejected.outcome == RunOutcome::OptimizationFailed);
    assert(!rejected.diagnostics.empty());
}

}  // namespace

int main() {
    testFixedPointAndSemantics();
    testBindingUpdatesAndInputEffects();
    testCseUsesValueVersionsAndPreservesTraps();
    testPureCseAndDce();
    testLimitsAndSnapshots();
    testConstantOperationEdgesAndInvalidIr();
    std::puts("optimizer tests passed");
}
