
#pragma once
#include <optional>
#include <vector>
#include "Avltree.hpp"
#include "Diagnostics.hpp"
#include "Ir.hpp"
#include "Optimizertypes.hpp"
#include "expression_dag.hpp"
#include "identifier_trie.hpp"

enum class RunOutcome { FrontendFailed, OptimizationFailed, Optimized };

struct ExprMetadata {
    available_sheep::ExprId dagNodeId = 0;
    std::string resultName;
    std::size_t useCount = 0;
};

struct AvlNodeSnapshot {
    available_sheep::ExprId id = 0;
    ExprMetadata info;
    int height = 1;
    std::optional<available_sheep::ExprId> leftChild;
    std::optional<available_sheep::ExprId> rightChild;
};

using RotationEvent = AvlRotationEvent;

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
 