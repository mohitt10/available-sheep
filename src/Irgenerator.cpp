#include "Irgenerator.hpp"

namespace {

class Generator {
public:
    Program generate(const Ast& ast) {
        for (const Stmt& s : ast.statements) {
            if (s.kind == StmtKind::Declare) {
                SymbolInfo info;
                info.id = s.symbol;
                info.name = s.name;
                program.symbols.push_back(info);
            }
            Operand value = genExpr(*s.expr);
            if (s.kind == StmtKind::Print)
                emit(Opcode::Print, Operand::none(), value, Operand::none(), s.line, s.column);
            else
                emit(Opcode::Copy, Operand::symbol(s.symbol), value, Operand::none(), s.line, s.column);
        }
        return program;
    }

private:
    Program program;

    Operand newTemp() { return Operand::temp(program.tempCount++); }

    void emit(Opcode op, Operand dst, Operand a, Operand b, int line, int column) {
        program.code.push_back(makeInstruction(op, dst, a, b, line, column));
    }

    Operand genExpr(const Expr& e) {
        switch (e.kind) {
            case ExprKind::IntLiteral:
                return Operand::constant(e.value);
            case ExprKind::Variable:
                return Operand::symbol(e.symbol);
            case ExprKind::Input: {
                Operand t = newTemp();
                emit(Opcode::Input, t, Operand::none(), Operand::none(), e.line, e.column);
                return t;
            }
            case ExprKind::Unary: {
                Operand a = genExpr(*e.left);
                Operand t = newTemp();
                emit(e.op, t, a, Operand::none(), e.line, e.column);
                return t;
            }
            case ExprKind::Binary: {
                Operand a = genExpr(*e.left);
                Operand b = genExpr(*e.right);
                Operand t = newTemp();
                emit(e.op, t, a, b, e.line, e.column);
                return t;
            }
        }
        return Operand::none();
    }
};

}

Program generateIr(const Ast& ast) {
    Generator g;
    return g.generate(ast);
}