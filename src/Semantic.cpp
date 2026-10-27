#include "Semantic.hpp"
#include <set>

namespace {

void report(std::vector<Diagnostic>& diags, int line, int column, const std::string& message) {
    Diagnostic d;
    d.severity = Severity::Error;
    d.stage = DiagStage::Semantic;
    d.line = line;
    d.column = column;
    d.message = message;
    diags.push_back(d);
}

void checkExpr(const Expr* e, const std::set<SymbolId>& declared, std::vector<Diagnostic>& diags) {
    if (!e) return;
    if (e->kind == ExprKind::Variable && !declared.count(e->symbol))
        report(diags, e->line, e->column, "undeclared variable '" + e->name + "'");
    checkExpr(e->left.get(), declared, diags);
    checkExpr(e->right.get(), declared, diags);
}

}

void analyze(const Ast& ast, std::vector<Diagnostic>& diagnostics) {
    std::set<SymbolId> declared;
    for (const Stmt& s : ast.statements) {
        checkExpr(s.expr.get(), declared, diagnostics);
        if (s.kind == StmtKind::Declare) {
            if (declared.count(s.symbol))
                report(diagnostics, s.line, s.column, "variable '" + s.name + "' is already declared");
            declared.insert(s.symbol);
        } else if (s.kind == StmtKind::Assign) {
            if (!declared.count(s.symbol))
                report(diagnostics, s.line, s.column, "assignment to undeclared variable '" + s.name + "'");
        }
    }
}