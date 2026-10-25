#include "Interpreter.hpp"
#include <map>

RunResult run(const Program& program, const std::vector<std::int32_t>& input) {
    RunResult result;
    if (!validate(program).empty()) {
        result.status = RunStatus::InvalidProgram;
        return result;
    }

    std::vector<std::int32_t> temps(program.tempCount, 0);
    std::map<SymbolId, std::int32_t> symbols;
    std::size_t nextInput = 0;

    auto read = [&](const Operand& o) -> std::int32_t {
        switch (o.kind) {
            case OperandKind::Const: return o.imm;
            case OperandKind::Temp: return temps[o.id];
            case OperandKind::Symbol: return symbols[o.id];
            case OperandKind::None: return 0;
        }
        return 0;
    };
    auto write = [&](const Operand& o, std::int32_t v) {
        if (o.kind == OperandKind::Temp) temps[o.id] = v;
        else if (o.kind == OperandKind::Symbol) symbols[o.id] = v;
    };

    for (const Instruction& ins : program.code) {
        std::int32_t value = 0;
        if (ins.op == Opcode::Print) {
            result.output.push_back(read(ins.a));
        } else if (ins.op == Opcode::Input) {
            if (nextInput >= input.size()) {
                result.status = RunStatus::InputExhausted;
                return result;
            }
            write(ins.dst, input[nextInput++]);
        } else if (operandCount(ins.op) == 1) {
            evalUnary(ins.op, read(ins.a), value);
            write(ins.dst, value);
        } else {
            if (!evalBinary(ins.op, read(ins.a), read(ins.b), value)) {
                result.status = RunStatus::DivisionByZero;
                return result;
            }
            write(ins.dst, value);
        }
    }
    return result;
}