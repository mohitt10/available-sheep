#include "Lexer.hpp"
#include <cctype>
#include <climits>

namespace {

std::string describeChar(char c) {
    unsigned char u = static_cast<unsigned char>(c);
    if (u >= 32 && u < 127) return std::string("'") + c + "'";
    const char* hex = "0123456789ABCDEF";
    return std::string("byte 0x") + hex[u >> 4] + hex[u & 15];
}

bool isIdentStart(char c) {
    return std::isalpha(static_cast<unsigned char>(c)) || c == '_';
}

bool isIdentPart(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
}

bool isDigit(char c) {
    return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

}

std::vector<Token> tokenize(const std::string& src, SymbolRegistry& registry,
                            std::vector<Diagnostic>& diagnostics) {
    std::vector<Token> tokens;
    std::size_t i = 0;
    int line = 1;
    int column = 1;

    auto advance = [&]() {
        if (src[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
        i++;
    };
    auto report = [&](int l, int c, const std::string& message) {
        Diagnostic d;
        d.severity = Severity::Error;
        d.stage = DiagStage::Lexical;
        d.line = l;
        d.column = c;
        d.message = message;
        diagnostics.push_back(d);
    };
    auto peekIs = [&](char c) { return i + 1 < src.size() && src[i + 1] == c; };

    while (i < src.size()) {
        char c = src[i];

        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        if (c == '/' && peekIs('/')) {
            while (i < src.size() && src[i] != '\n') advance();
            continue;
        }

        if (c == '/' && peekIs('*')) {
            int startLine = line;
            int startColumn = column;
            advance();
            advance();
            bool closed = false;
            while (i < src.size()) {
                if (src[i] == '*' && peekIs('/')) {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) report(startLine, startColumn, "unterminated block comment");
            continue;
        }

        Token t;
        t.line = line;
        t.column = column;

        if (isDigit(c)) {
            long long value = 0;
            bool tooBig = false;
            while (i < src.size() && isDigit(src[i])) {
                t.text += src[i];
                if (!tooBig) {
                    value = value * 10 + (src[i] - '0');
                    if (value > INT_MAX) tooBig = true;
                }
                advance();
            }
            if (tooBig) {
                report(t.line, t.column, "integer literal '" + t.text + "' is too large for int");
                continue;
            }
            t.kind = TokenKind::Int;
            t.value = static_cast<std::int32_t>(value);
            tokens.push_back(t);
            continue;
        }

        if (isIdentStart(c)) {
            while (i < src.size() && isIdentPart(src[i])) {
                t.text += src[i];
                advance();
            }
            if (t.text == "int") t.kind = TokenKind::KwInt;
            else if (t.text == "print") t.kind = TokenKind::KwPrint;
            else if (t.text == "input") t.kind = TokenKind::KwInput;
            else {
                t.kind = TokenKind::Ident;
                t.symbol = registry.idFor(t.text);
            }
            tokens.push_back(t);
            continue;
        }

        bool twoChar = true;
        if (c == '<' && peekIs('=')) t.kind = TokenKind::Le;
        else if (c == '>' && peekIs('=')) t.kind = TokenKind::Ge;
        else if (c == '=' && peekIs('=')) t.kind = TokenKind::EqEq;
        else if (c == '!' && peekIs('=')) t.kind = TokenKind::Ne;
        else twoChar = false;

        if (twoChar) {
            t.text = src.substr(i, 2);
            advance();
            advance();
            tokens.push_back(t);
            continue;
        }

        bool known = true;
        switch (c) {
            case '+': t.kind = TokenKind::Plus; break;
            case '-': t.kind = TokenKind::Minus; break;
            case '*': t.kind = TokenKind::Star; break;
            case '/': t.kind = TokenKind::Slash; break;
            case '%': t.kind = TokenKind::Percent; break;
            case '<': t.kind = TokenKind::Lt; break;
            case '>': t.kind = TokenKind::Gt; break;
            case '!': t.kind = TokenKind::Not; break;
            case '=': t.kind = TokenKind::Assign; break;
            case '(': t.kind = TokenKind::LParen; break;
            case ')': t.kind = TokenKind::RParen; break;
            case ';': t.kind = TokenKind::Semi; break;
            default: known = false;
        }

        if (!known) {
            report(line, column, "unexpected character " + describeChar(c));
            advance();
            continue;
        }

        t.text = std::string(1, c);
        advance();
        tokens.push_back(t);
    }

    Token end;
    end.kind = TokenKind::End;
    end.line = line;
    end.column = column;
    tokens.push_back(end);
    return tokens;
}