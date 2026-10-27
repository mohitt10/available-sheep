
#pragma once
#include <vector>
#include "Avltree.hpp"
#include "Diagnostics.hpp"
#include "Ir.hpp"
#include "Optimizertypes.hpp"
#include "expression_dag.hpp"
#include "identifier_trie.hpp"

enum class RunOutcome { FrontendFailed, OptimizationFailed, Optimized };

struct ExprMetadata {
    int dagNodeId = -1;
    std::string resultName;
    int useCount = 0;
};

struct AvlNodeSnapshot {
    std::string key;
    ExprMetadata info;
    int height = 1;
};

struct RotationEvent {
    std::string description;
};

struct OptimizationResult {
    RunOutcome outcome = RunOutcome::FrontendFailed;
    std::vector<Diagnostic> diagnostics;
 
    Program initialIr;
    Program optimizedIr;
 
    Convergence convergence = Convergence::Converged;
    int iterationsRun = 0;
    std::vector<IterationReport> iterations;
    IrMetrics before;
    IrMetrics after;
 
    std::vector<ExprMetadata> avlOrdered;
    std::vector<AvlNodeSnapshot> avlNodes;
    std::vector<RotationEvent> avlRotations;
    std::vector<available_sheep::ExpressionNodeSnapshot> dagNodes;
    std::vector<available_sheep::TrieEntry> trieEntries;
};
 