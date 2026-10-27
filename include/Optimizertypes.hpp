#pragma once
#include <string>
#include <vector>
 
enum class PassKind {
    ConstantFolding,
    ConstantPropagation,
    CommonSubexpressionElimination,
    DeadCodeElimination
};
 
constexpr int kPassCount = 4;
 
inline const char* passName(PassKind pass) {
    switch (pass) {
        case PassKind::ConstantFolding: return "Constant folding";
        case PassKind::ConstantPropagation: return "Constant propagation";
        case PassKind::CommonSubexpressionElimination: return "Common subexpression elimination";
        case PassKind::DeadCodeElimination: return "Dead code elimination";
    }
    return "";
}
 
struct PassOptions {
    bool enabled[kPassCount] = {true, true, true, true};
    int maxIterations = 10;
};
 
struct PassStats {
    PassKind pass = PassKind::ConstantFolding;
    bool changed = false;
    int instructionsRewritten = 0;
    int instructionsRemoved = 0;
};
 
struct IterationReport {
    int iteration = 0;
    std::vector<PassStats> passes;
    bool changed = false;
};
 
enum class Convergence { Converged, IterationLimit, RepeatedState };
 
struct IrMetrics {
    int instructions = 0;
    int copies = 0;
    int unaryOps = 0;
    int binaryOps = 0;
    int inputs = 0;
    int prints = 0;
};
 
struct Program;
IrMetrics computeMetrics(const Program& program);
 