#include "Parser.hpp"

#include <utility>

namespace {

struct ParseError {};

const int kMaxDepth = 200;

using ExprPtr = std::unique_ptr<Expr>;

class Parser {
public:
    Parser(const std::vector<Token>& t, std::vector<Diagnostic>& d) : tokens(t), diags(d) {}

    Ast parseProgram() {
        Ast ast;
        while (!at(TokenKind::End)) {
            try {
                ast.statements.push_back(parseStatement());
            } catch (const ParseError&) {
                depth = 0;
                synchronize();
            }
        }
        return ast;
    }

private:
    const std::vector<Token>& tokens;
    std::vector<Diagnostic>& diags;
    std::size_t pos = 0;
    int depth = 0;

    struct DepthGuard {
        int& d;
        explicit DepthGuard(int& x) : d(x) { d++; }
        ~DepthGuard() { d--; }
    };

    const Token& cur() const { return tokens[pos]; }
    bool at(TokenKind k) const { return cur().kind == k; }

    Token advance() {
        Token t = tokens[pos];
        if (pos + 1 < tokens.size()) pos++;
        return t;
    }

    static std::string describe(const Token& t) {
        switch (t.kind) {
            case TokenKind::End: return "end of input";
            case TokenKind::Ident: return "identifier '" + t.text + "'";
            case TokenKind::Int: return "number '" + t.text + "'";
            default: return "'" + t.text + "'";
        }
    }

    [[noreturn]] void fail(const Token& at, const std::string& message) {
        Diagnostic d;
        d.severity = Severity::Error;
        d.stage = DiagStage::Syntax;
        d.line = at.line;
        d.column = at.column;
        d.message = message;
        diags.push_back(d);
        throw ParseError();
    }

    Token expect(TokenKind kind, const std::string& what) {
        if (!at(kind)) fail(cur(), "expected " + what + " but found " + describe(cur()));
        return advance();
    }

    void synchronize() {
        while (!at(TokenKind::End) && !at(TokenKind::Semi)) advance();
        if (at(TokenKind::Semi)) advance();
    }

    Stmt parseStatement() {
        Stmt s;
        s.line = cur().line;
        s.column = cur().column;

        if (at(TokenKind::KwInt)) {
            advance();
            Token name = expect(TokenKind::Ident, "a variable name after 'int'");
            if (at(TokenKind::Semi)) fail(cur(), "declaration of '" + name.text + "' needs an initializer");
            expect(TokenKind::Assign, "'='");
            s.kind = StmtKind::Declare;
            s.symbol = name.symbol;
            s.name = name.text;
            s.expr = parseExpr();
            expect(TokenKind::Semi, "';'");
            return s;
        }

        if (at(TokenKind::Ident)) {
            Token name = advance();
            if (!at(TokenKind::Assign)) fail(cur(), "expected '=' after '" + name.text + "' but found " + describe(cur()));
            advance();
            s.kind = StmtKind::Assign;
            s.symbol = name.symbol;
            s.name = name.text;
            s.expr = parseExpr();
            expect(TokenKind::Semi, "';'");
            return s;
        }

        if (at(TokenKind::KwPrint)) {
            advance();
            expect(TokenKind::LParen, "'(' after 'print'");
            s.kind = StmtKind::Print;
            s.expr = parseExpr();
            expect(TokenKind::RParen, "')'");
            expect(TokenKind::Semi, "';'");
            return s;
        }

        fail(cur(), "expected a statement but found " + describe(cur()));
    }

    ExprPtr makeBinary(Opcode op, ExprPtr l, ExprPtr r, const Token& opToken) {
        ExprPtr e(new Expr());
        e->kind = ExprKind::Binary;
        e->op = op;
        e->line = opToken.line;
        e->column = opToken.column;
        e->left = std::move(l);
        e->right = std::move(r);
        return e;
    }

    ExprPtr parseExpr() { return parseEquality(); }

    ExprPtr parseEquality() {
        ExprPtr left = parseRelational();
        while (at(TokenKind::EqEq) || at(TokenKind::Ne)) {
            Token op = advance();
            ExprPtr right = parseRelational();
            left = makeBinary(op.kind == TokenKind::EqEq ? Opcode::Eq : Opcode::Ne,
                              std::move(left), std::move(right), op);
        }
        return left;
    }

    ExprPtr parseRelational() {
        ExprPtr left = parseAdditive();
        while (at(TokenKind::Lt) || at(TokenKind::Le) || at(TokenKind::Gt) || at(TokenKind::Ge)) {
            Token op = advance();
            ExprPtr right = parseAdditive();
            Opcode code = Opcode::Lt;
            if (op.kind == TokenKind::Le) code = Opcode::Le;
            else if (op.kind == TokenKind::Gt) code = Opcode::Gt;
            else if (op.kind == TokenKind::Ge) code = Opcode::Ge;
            left = makeBinary(code, std::move(left), std::move(right), op);
        }
        return left;
    }

    ExprPtr parseAdditive() {
        ExprPtr left = parseTerm();
        while (at(TokenKind::Plus) || at(TokenKind::Minus)) {
            Token op = advance();
            ExprPtr right = parseTerm();
            left = makeBinary(op.kind == TokenKind::Plus ? Opcode::Add : Opcode::Sub,
                              std::move(left), std::move(right), op);
        }
        return left;
    }

    ExprPtr parseTerm() {
        ExprPtr left = parseUnary();
        while (at(TokenKind::Star) || at(TokenKind::Slash) || at(TokenKind::Percent)) {
            Token op = advance();
            ExprPtr right = parseUnary();
            Opcode code = Opcode::Mul;
            if (op.kind == TokenKind::Slash) code = Opcode::Div;
            else if (op.kind == TokenKind::Percent) code = Opcode::Mod;
            left = makeBinary(code, std::move(left), std::move(right), op);
        }
        return left;
    }

    ExprPtr parseUnary() {
        if (at(TokenKind::Minus) || at(TokenKind::Not)) {
            DepthGuard guard(depth);
            if (depth > kMaxDepth) fail(cur(), "expression is nested too deeply");
            Token op = advance();
            ExprPtr e(new Expr());
            e->kind = ExprKind::Unary;
            e->op = (op.kind == TokenKind::Minus) ? Opcode::Neg : Opcode::Not;
            e->line = op.line;
            e->column = op.column;
            e->left = parseUnary();
            return e;
        }
        return parsePrimary();
    }

    ExprPtr parsePrimary() {
        ExprPtr e(new Expr());
        e->line = cur().line;
        e->column = cur().column;

        if (at(TokenKind::Int)) {
            Token t = advance();
            e->kind = ExprKind::IntLiteral;
            e->value = t.value;
            return e;
        }
        if (at(TokenKind::Ident)) {
            Token t = advance();
            e->kind = ExprKind::Variable;
            e->symbol = t.symbol;
            e->name = t.text;
            return e;
        }
        if (at(TokenKind::KwInput)) {
            advance();
            expect(TokenKind::LParen, "'(' after 'input'");
            expect(TokenKind::RParen, "')' after 'input('");
            e->kind = ExprKind::Input;
            return e;
        }
        if (at(TokenKind::LParen)) {
            DepthGuard guard(depth);
            if (depth > kMaxDepth) fail(cur(), "expression is nested too deeply");
            advance();
            ExprPtr inner = parseExpr();
            expect(TokenKind::RParen, "')'");
            return inner;
        }
        fail(cur(), "expected an expression but found " + describe(cur()));
    }
};

}

Ast parse(const std::vector<Token>& tokens, std::vector<Diagnostic>& diagnostics) {
    if (tokens.empty() || tokens.back().kind != TokenKind::End) {
        Diagnostic diagnostic;
        diagnostic.severity = Severity::Error;
        diagnostic.stage = DiagStage::Syntax;
        diagnostic.message = "token stream is missing its end-of-input marker";
        if (!tokens.empty()) {
            diagnostic.line = tokens.back().line;
            diagnostic.column = tokens.back().column;
        }
        diagnostics.push_back(std::move(diagnostic));
        return {};
    }
    for (std::size_t i = 0; i + 1 < tokens.size(); ++i) {
        if (tokens[i].kind != TokenKind::End) continue;
        Diagnostic diagnostic;
        diagnostic.severity = Severity::Error;
        diagnostic.stage = DiagStage::Syntax;
        diagnostic.line = tokens[i].line;
        diagnostic.column = tokens[i].column;
        diagnostic.message = "unexpected end-of-input marker before the end of the token stream";
        diagnostics.push_back(std::move(diagnostic));
        return {};
    }
    Parser parser(tokens, diagnostics);
    return parser.parseProgram();
}