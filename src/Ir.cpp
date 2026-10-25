#include "Ir.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include "Optimizertypes.hpp"

Operand Operand::none() { return Operand(); }

Operand Operand::constant(std::int32_t value) {
    Operand o;
    o.kind = OperandKind::Const;
    o.imm = value;
    return o;
}

Operand Operand::temp(std::uint32_t id) {
    Operand o;
    o.kind = OperandKind::Temp;
    o.id = id;
    return o;
}

Operand Operand::symbol(SymbolId id) {
    Operand o;
    o.kind = OperandKind::Symbol;
    o.id = id;
    return o;
}

Effect effectOf(Opcode op) {
    switch (op) {
        case Opcode::Div:
        case Opcode::Mod: return Effect::MayTrap;
        case Opcode::Input: return Effect::ReadsInput;
        case Opcode::Print: return Effect::WritesOutput;
        default: return Effect::Pure;
    }
}

int operandCount(Opcode op) {
    switch (op) {
        case Opcode::Input: return 0;
        case Opcode::Copy:
        case Opcode::Neg:
        case Opcode::Not:
        case Opcode::Print: return 1;
        default: return 2;
    }
}

bool hasDestination(Opcode op) { return op != Opcode::Print; }

bool isCommutative(Opcode op) {
    return op == Opcode::Add || op == Opcode::Mul || op == Opcode::Eq || op == Opcode::Ne;
}

const char* opcodeText(Opcode op) {
    switch (op) {
        case Opcode::Neg: return "-";
        case Opcode::Not: return "!";
        case Opcode::Add: return "+";
        case Opcode::Sub: return "-";
        case Opcode::Mul: return "*";
        case Opcode::Div: return "/";
        case Opcode::Mod: return "%";
        case Opcode::Lt: return "<";
        case Opcode::Le: return "<=";
        case Opcode::Gt: return ">";
        case Opcode::Ge: return ">=";
        case Opcode::Eq: return "==";
        case Opcode::Ne: return "!=";
        case Opcode::Input: return "input";
        case Opcode::Print: return "print";
        case Opcode::Copy: return "";
    }
    return "";
}

Instruction makeInstruction(Opcode op, Operand dst, Operand a, Operand b, int line) {
    Instruction ins;
    ins.op = op;
    ins.dst = dst;
    ins.a = a;
    ins.b = b;
    ins.effect = effectOf(op);
    ins.line = line;
    return ins;
}

static std::int32_t wrap(std::uint32_t v) { return static_cast<std::int32_t>(v); }

bool evalUnary(Opcode op, std::int32_t a, std::int32_t& out) {
    switch (op) {
        case Opcode::Copy: out = a; return true;
        case Opcode::Neg: out = wrap(0u - static_cast<std::uint32_t>(a)); return true;
        case Opcode::Not: out = (a == 0) ? 1 : 0; return true;
        default: return false;
    }
}

bool evalBinary(Opcode op, std::int32_t a, std::int32_t b, std::int32_t& out) {
    std::uint32_t ua = static_cast<std::uint32_t>(a);
    std::uint32_t ub = static_cast<std::uint32_t>(b);
    const std::int32_t minInt = std::numeric_limits<std::int32_t>::min();
    switch (op) {
        case Opcode::Add: out = wrap(ua + ub); return true;
        case Opcode::Sub: out = wrap(ua - ub); return true;
        case Opcode::Mul: out = wrap(ua * ub); return true;
        case Opcode::Div:
            if (b == 0) return false;
            out = (a == minInt && b == -1) ? minInt : a / b;
            return true;
        case Opcode::Mod:
            if (b == 0) return false;
            out = (b == -1) ? 0 : a % b;
            return true;
        case Opcode::Lt: out = a < b; return true;
        case Opcode::Le: out = a <= b; return true;
        case Opcode::Gt: out = a > b; return true;
        case Opcode::Ge: out = a >= b; return true;
        case Opcode::Eq: out = a == b; return true;
        case Opcode::Ne: out = a != b; return true;
        default: return false;
    }
}

std::string symbolName(const Program& program, SymbolId id) {
    for (const SymbolInfo& s : program.symbols)
        if (s.id == id) return s.name;
    return "$" + std::to_string(id);
}

std::string toString(const Operand& operand, const Program& program) {
    switch (operand.kind) {
        case OperandKind::Const: return std::to_string(operand.imm);
        case OperandKind::Temp: return "t" + std::to_string(operand.id);
        case OperandKind::Symbol: return symbolName(program, operand.id);
        case OperandKind::None: return "_";
    }
    return "_";
}

std::string toString(const Instruction& ins, const Program& program) {
    std::string a = toString(ins.a, program);
    std::string b = toString(ins.b, program);
    switch (ins.op) {
        case Opcode::Print: return "print " + a;
        case Opcode::Input: return toString(ins.dst, program) + " = input()";
        case Opcode::Copy: return toString(ins.dst, program) + " = " + a;
        case Opcode::Neg:
        case Opcode::Not: return toString(ins.dst, program) + " = " + opcodeText(ins.op) + a;
        default:
            return toString(ins.dst, program) + " = " + a + " " + opcodeText(ins.op) + " " + b;
    }
}

std::string toString(const Program& program) {
    std::string out;
    for (std::size_t i = 0; i < program.code.size(); i++)
        out += std::to_string(i) + ": " + toString(program.code[i], program) + "\n";
    return out;
}

static bool symbolKnown(const Program& p, SymbolId id) {
    return std::any_of(p.symbols.begin(), p.symbols.end(),
                       [&](const SymbolInfo& s) { return s.id == id; });
}

std::vector<std::string> validate(const Program& p) {
    std::vector<std::string> errors;
    std::set<std::uint32_t> definedTemps;
    std::set<SymbolId> assigned;

    for (std::size_t i = 0; i < p.code.size(); i++) {
        const Instruction& ins = p.code[i];
        auto fail = [&](const std::string& msg) {
            errors.push_back("instruction " + std::to_string(i) + ": " + msg);
        };

        if (ins.effect != effectOf(ins.op)) fail("effect does not match opcode");

        int n = operandCount(ins.op);
        const Operand* operands[2] = {&ins.a, &ins.b};
        for (int k = 0; k < 2; k++) {
            const Operand& o = *operands[k];
            if (k >= n) {
                if (o.kind != OperandKind::None) fail("unexpected operand");
                continue;
            }
            if (o.kind == OperandKind::None) fail("missing operand");
            if (o.kind == OperandKind::Temp && !definedTemps.count(o.id))
                fail("temp used before definition");
            if (o.kind == OperandKind::Symbol) {
                if (!symbolKnown(p, o.id)) fail("unknown symbol");
                else if (!assigned.count(o.id)) fail("symbol used before assignment");
            }
        }

        if (!hasDestination(ins.op)) {
            if (ins.dst.kind != OperandKind::None) fail("unexpected destination");
            continue;
        }
        if (ins.dst.kind == OperandKind::Temp) {
            if (ins.dst.id >= p.tempCount) fail("temp id out of range");
            if (!definedTemps.insert(ins.dst.id).second) fail("temp defined twice");
        } else if (ins.dst.kind == OperandKind::Symbol) {
            if (!symbolKnown(p, ins.dst.id)) fail("unknown destination symbol");
            assigned.insert(ins.dst.id);
        } else {
            fail("destination must be a temp or symbol");
        }
    }
    return errors;
}


IrMetrics computeMetrics(const Program& program) {
    IrMetrics m;
    m.instructions = static_cast<int>(program.code.size());
    for (const Instruction& ins : program.code) {
        if (ins.op == Opcode::Copy) m.copies++;
        else if (ins.op == Opcode::Input) m.inputs++;
        else if (ins.op == Opcode::Print) m.prints++;
        else if (operandCount(ins.op) == 1) m.unaryOps++;
        else m.binaryOps++;
    }
    return m;
}