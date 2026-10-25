#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include "Ir.hpp"
#include "shared_types.hpp"
 
enum class ExprKind { IntLiteral, Variable, Input, Unary, Binary };
 
struct Expr {
    ExprKind kind = ExprKind::IntLiteral;
    int line = 0;
    int column = 0;
    std::int32_t value = 0;
    SymbolId symbol = 0;
    std::string name;
    Opcode op = Opcode::Copy;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};
 
enum class StmtKind { Declare, Assign, Print };
 
struct Stmt {
    StmtKind kind = StmtKind::Print;
    int line = 0;
    int column = 0;
    SymbolId symbol = 0;
    std::string name;
    std::unique_ptr<Expr> expr;
};
 
struct Ast {
    std::vector<Stmt> statements;
};
 