#pragma once

#include <memory>

#include "Resultmodel.hpp"

class OptimizationPass {
public:
    virtual ~OptimizationPass() = default;
    virtual PassKind kind() const noexcept = 0;
    virtual PassStats run(Program& program) const = 0;
};

std::unique_ptr<OptimizationPass> makeOptimizationPass(PassKind kind);

OptimizationResult optimizeProgram(
    const Program& initialIr, const PassOptions& options = PassOptions{},
    const available_sheep::IdentifierTrie* trie = nullptr);
