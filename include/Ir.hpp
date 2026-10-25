#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "shared_types.hpp"
 
enum class Opcode {
    Copy, Neg, Not,
    Add, Sub, Mul, Div, Mod,
    Lt, Le, Gt, Ge, Eq, Ne,
    Input, Print
};
 
enum class Effect { Pure, MayTrap, ReadsInput, WritesOutput };
 
enum class OperandKind { None, Const, Temp, Symbol };
 
struct Operand {
    OperandKind kind = OperandKind::None;
    std::int32_t imm = 0;
    std::uint32_t id = 0;
 
    static Operand none();
    static Operand constant(std::int32_t value);
    static Operand temp(std::uint32_t id);
    static Operand symbol(SymbolId id);
 
    bool isConst() const { return kind == OperandKind::Const; }
    bool operator==(const Operand& o) const { return kind == o.kind && imm == o.imm && id == o.id; }
    bool operator!=(const Operand& o) const { return !(*this == o); }
};
 
struct Instruction {
    Opcode op = Opcode::Copy;
    Operand dst;
    Operand a;
    Operand b;
    Effect effect = Effect::Pure;
    int line = 0;
};
 
struct SymbolInfo {
    SymbolId id = 0;
    std::string name;
};
 
struct Program {
    std::vector<SymbolInfo> symbols;
    std::vector<Instruction> code;
    std::uint32_t tempCount = 0;
};
 
Instruction makeInstruction(Opcode op, Operand dst, Operand a = Operand(), Operand b = Operand(), int line = 0);
 
Effect effectOf(Opcode op);
int operandCount(Opcode op);
bool hasDestination(Opcode op);
bool isCommutative(Opcode op);
const char* opcodeText(Opcode op);
 
bool evalUnary(Opcode op, std::int32_t a, std::int32_t& out);
bool evalBinary(Opcode op, std::int32_t a, std::int32_t b, std::int32_t& out);
 
std::string symbolName(const Program& program, SymbolId id);
std::string toString(const Operand& operand, const Program& program);
std::string toString(const Instruction& instruction, const Program& program);
std::string toString(const Program& program);
 
std::vector<std::string> validate(const Program& program);