#include "Diagnostics.hpp"
#include "Interpreter.hpp"
#include "Ir.hpp"
#include "Optimizertypes.hpp"
#include "Resultmodel.hpp"
#include <cassert>
#include <climits>
#include <cstdio>

static Program sampleProgram() {
    Program p;
    p.symbols = {{1, "x"}, {2, "y"}};
    p.tempCount = 4;
    Operand x = Operand::symbol(1);
    Operand y = Operand::symbol(2);
    Operand t0 = Operand::temp(0), t1 = Operand::temp(1), t2 = Operand::temp(2), t3 = Operand::temp(3);
    p.code = {
        makeInstruction(Opcode::Add, t0, Operand::constant(2), Operand::constant(3), 1),
        makeInstruction(Opcode::Copy, x, t0, Operand::none(), 1),
        makeInstruction(Opcode::Input, t1, Operand::none(), Operand::none(), 2),
        makeInstruction(Opcode::Mul, t2, x, t1, 2),
        makeInstruction(Opcode::Copy, y, t2, Operand::none(), 2),
        makeInstruction(Opcode::Div, t3, y, x, 3),
        makeInstruction(Opcode::Print, Operand::none(), t3, Operand::none(), 3),
    };
    return p;
}

static void testPrint() {
    Program p = sampleProgram();
    std::string expected =
        "0: t0 = 2 + 3\n"
        "1: x = t0\n"
        "2: t1 = input()\n"
        "3: t2 = x * t1\n"
        "4: y = t2\n"
        "5: t3 = y / x\n"
        "6: print t3\n";
    assert(toString(p) == expected);
}

static void testValidate() {
    Program p = sampleProgram();
    assert(validate(p).empty());

    Program a = p;
    a.code[3].a = Operand::temp(3);
    assert(!validate(a).empty());

    Program b = p;
    b.code[0].a = Operand::symbol(2);
    assert(!validate(b).empty());

    Program c = p;
    c.code[5].effect = Effect::Pure;
    assert(!validate(c).empty());

    Program d = p;
    d.code[2].dst = Operand::temp(0);
    assert(!validate(d).empty());

    Program e = p;
    e.code[6].dst = Operand::temp(0);
    assert(!validate(e).empty());
}

static void testEffects() {
    assert(effectOf(Opcode::Add) == Effect::Pure);
    assert(effectOf(Opcode::Div) == Effect::MayTrap);
    assert(effectOf(Opcode::Mod) == Effect::MayTrap);
    assert(effectOf(Opcode::Input) == Effect::ReadsInput);
    assert(effectOf(Opcode::Print) == Effect::WritesOutput);
    assert(isCommutative(Opcode::Add) && isCommutative(Opcode::Mul));
    assert(!isCommutative(Opcode::Sub) && !isCommutative(Opcode::Lt));
}

static void testEval() {
    std::int32_t out = 0;
    assert(evalBinary(Opcode::Add, INT_MAX, 1, out) && out == INT_MIN);
    assert(evalBinary(Opcode::Mul, 65536, 65536, out) && out == 0);
    assert(evalBinary(Opcode::Div, 7, -2, out) && out == -3);
    assert(evalBinary(Opcode::Mod, 7, -2, out) && out == 1);
    assert(evalBinary(Opcode::Div, INT_MIN, -1, out) && out == INT_MIN);
    assert(evalBinary(Opcode::Mod, INT_MIN, -1, out) && out == 0);
    assert(!evalBinary(Opcode::Div, 1, 0, out));
    assert(!evalBinary(Opcode::Mod, 1, 0, out));
    assert(evalBinary(Opcode::Lt, 1, 2, out) && out == 1);
    assert(evalUnary(Opcode::Neg, INT_MIN, out) && out == INT_MIN);
    assert(evalUnary(Opcode::Not, 5, out) && out == 0);
    assert(evalUnary(Opcode::Not, 0, out) && out == 1);
}

static void testRun() {
    Program p = sampleProgram();
    RunResult r = run(p, {4});
    assert(r.status == RunStatus::Ok);
    assert(r.output.size() == 1 && r.output[0] == 4);

    RunResult none = run(p, {});
    assert(none.status == RunStatus::InputExhausted && none.output.empty());

    Program z;
    z.tempCount = 1;
    z.code = {
        makeInstruction(Opcode::Print, Operand::none(), Operand::constant(9)),
        makeInstruction(Opcode::Div, Operand::temp(0), Operand::constant(1), Operand::constant(0)),
        makeInstruction(Opcode::Print, Operand::none(), Operand::constant(8)),
    };
    assert(validate(z).empty());
    RunResult trap = run(z, {});
    assert(trap.status == RunStatus::DivisionByZero);
    assert(trap.output.size() == 1 && trap.output[0] == 9);
}

static void testMetrics() {
    IrMetrics m = computeMetrics(sampleProgram());
    assert(m.instructions == 7 && m.copies == 2 && m.binaryOps == 3);
    assert(m.inputs == 1 && m.prints == 1 && m.unaryOps == 0);
}

static void testResultModel() {
    OptimizationResult r;
    assert(r.outcome == RunOutcome::FrontendFailed);
    assert(r.initialIr.code.empty() && r.optimizedIr.code.empty());
    PassOptions o;
    assert(o.maxIterations == 10);
    for (int i = 0; i < kPassCount; i++) assert(o.enabled[i]);
}

int main() {
    testPrint();
    testValidate();
    testEffects();
    testEval();
    testRun();
    testMetrics();
    testResultModel();
    std::puts("ir tests passed");
}