#pragma once

#include <string>

enum class Severity { Error, Warning, Note };
enum class DiagStage { Lexical, Syntax, Semantic };

struct Diagnostic {
    Severity severity = Severity::Error;
    DiagStage stage = DiagStage::Lexical;
    std::string message;
    int line = 0;
    int column = 0;
};

using DiagnosticSeverity = Severity;