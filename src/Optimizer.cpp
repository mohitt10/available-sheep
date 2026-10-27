#include "Optimizer.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "expression_dag.hpp"

namespace {

using available_sheep::ExprId;
using available_sheep::ExpressionDag;
using available_sheep::ExpressionKey;
using available_sheep::OperatorKind;

constexpr available_sheep::TypeId kInt32Type = 1;

int passIndex(PassKind pass) {
    switch (pass) {
        case PassKind::ConstantFolding: return 0;
        case PassKind::ConstantPropagation: return 1;
        case PassKind::CommonSubexpressionElimination: return 2;
        case PassKind::DeadCodeElimination: return 3;
    }
    return -1;
}

bool isBinary(Opcode op) {
    return operandCount(op) == 2;
}

class ConstantFoldingPass final : public OptimizationPass {
public:
    PassKind kind() const noexcept override { return PassKind::ConstantFolding; }

    PassStats run(Program& program) const override {
        PassStats stats;
        stats.pass = kind();
        for (Instruction& instruction : program.code) {
            std::int32_t value = 0;
            bool evaluated = false;
            if (instruction.op == Opcode::Neg || instruction.op == Opcode::Not) {
                if (instruction.a.isConst())
                    evaluated = evalUnary(instruction.op, instruction.a.imm, value);
            } else if (isBinary(instruction.op) && instruction.a.isConst() &&
                       instruction.b.isConst()) {
                evaluated = evalBinary(instruction.op, instruction.a.imm,
                                       instruction.b.imm, value);
            }
            if (!evaluated) continue;

            instruction = makeInstruction(
                Opcode::Copy, instruction.dst, Operand::constant(value),
                Operand::none(), instruction.line, instruction.column);
            stats.changed = true;
            ++stats.instructionsRewritten;
        }
        return stats;
    }
};

class ConstantPropagationPass final : public OptimizationPass {
public:
    PassKind kind() const noexcept override { return PassKind::ConstantPropagation; }

    PassStats run(Program& program) const override {
        PassStats stats;
        stats.pass = kind();
        std::map<SymbolId, std::int32_t> symbols;
        std::map<std::uint32_t, std::int32_t> temps;

        for (Instruction& instruction : program.code) {
            bool rewritten = false;
            auto propagate = [&](Operand& operand) {
                if (operand.kind == OperandKind::Symbol) {
                    const auto found = symbols.find(operand.id);
                    if (found != symbols.end()) {
                        operand = Operand::constant(found->second);
                        rewritten = true;
                    }
                } else if (operand.kind == OperandKind::Temp) {
                    const auto found = temps.find(operand.id);
                    if (found != temps.end()) {
                        operand = Operand::constant(found->second);
                        rewritten = true;
                    }
                }
            };
            const int count = operandCount(instruction.op);
            if (count > 0) propagate(instruction.a);
            if (count > 1) propagate(instruction.b);
            if (rewritten) {
                stats.changed = true;
                ++stats.instructionsRewritten;
            }

            if (!hasDestination(instruction.op)) continue;
            if (instruction.dst.kind == OperandKind::Symbol) {
                if (instruction.op == Opcode::Copy && instruction.a.isConst())
                    symbols[instruction.dst.id] = instruction.a.imm;
                else
                    symbols.erase(instruction.dst.id);
            } else if (instruction.dst.kind == OperandKind::Temp) {
                if (instruction.op == Opcode::Copy && instruction.a.isConst())
                    temps[instruction.dst.id] = instruction.a.imm;
                else
                    temps.erase(instruction.dst.id);
            }
        }
        return stats;
    }
};

OperatorKind operatorKind(Opcode op) {
    switch (op) {
        case Opcode::Copy: return OperatorKind::Assign;
        case Opcode::Neg: return OperatorKind::Negate;
        case Opcode::Not: return OperatorKind::LogicalNot;
        case Opcode::Add: return OperatorKind::Add;
        case Opcode::Sub: return OperatorKind::Subtract;
        case Opcode::Mul: return OperatorKind::Multiply;
        case Opcode::Div: return OperatorKind::Divide;
        case Opcode::Mod: return OperatorKind::Remainder;
        case Opcode::Lt: return OperatorKind::Less;
        case Opcode::Le: return OperatorKind::LessEqual;
        case Opcode::Gt: return OperatorKind::Greater;
        case Opcode::Ge: return OperatorKind::GreaterEqual;
        case Opcode::Eq: return OperatorKind::Equal;
        case Opcode::Ne: return OperatorKind::NotEqual;
        case Opcode::Input:
        case Opcode::Print: return OperatorKind::Call;
    }
    return OperatorKind::Other;
}

ExpressionKey instructionKey(Opcode op, const std::vector<ExprId>& operands,
                             std::uint64_t ordinal = 0) {
    ExpressionKey key;
    key.op = operatorKind(op);
    key.type = kInt32Type;
    key.operands = operands;
    if (op == Opcode::Input || op == Opcode::Print)
        key.attributes.push_back(static_cast<std::uint64_t>(op));
    if (op == Opcode::Input || op == Opcode::Print)
        key.attributes.push_back(ordinal);
    return key;
}

class CommonSubexpressionPass final : public OptimizationPass {
public:
    PassKind kind() const noexcept override {
        return PassKind::CommonSubexpressionElimination;
    }

    PassStats run(Program& program) const override {
        PassStats stats;
        stats.pass = kind();
        ExpressionDag dag;
        std::map<std::int32_t, ExprId> constants;
        std::map<std::uint32_t, ExprId> temps;
        std::map<SymbolId, ExprId> symbols;
        std::map<ExprId, Operand> reusableResults;

        auto valueId = [&](const Operand& operand) -> ExprId {
            if (operand.kind == OperandKind::Const) {
                const auto found = constants.find(operand.imm);
                if (found != constants.end()) return found->second;
                ExpressionKey key;
                key.op = OperatorKind::Constant;
                key.type = kInt32Type;
                key.attributes.push_back(
                    static_cast<std::uint32_t>(operand.imm));
                const ExprId id = dag.addPureExpression(key);
                constants.emplace(operand.imm, id);
                return id;
            }
            if (operand.kind == OperandKind::Temp) return temps.at(operand.id);
            if (operand.kind == OperandKind::Symbol) return symbols.at(operand.id);
            throw std::logic_error("unexpected empty operand in optimizer");
        };

        for (std::size_t index = 0; index < program.code.size(); ++index) {
            Instruction& instruction = program.code[index];
            const int count = operandCount(instruction.op);
            std::vector<ExprId> inputs;
            if (count > 0) inputs.push_back(valueId(instruction.a));
            if (count > 1) inputs.push_back(valueId(instruction.b));

            ExprId resultId = 0;
            if (instruction.op == Opcode::Print) {
                ExpressionKey key = instructionKey(
                    instruction.op, inputs, static_cast<std::uint64_t>(index));
                dag.addEffectfulExpression(key);
                continue;
            }
            if (instruction.op == Opcode::Copy) {
                resultId = inputs.front();
            } else if (instruction.op == Opcode::Input) {
                ExpressionKey key = instructionKey(
                    instruction.op, inputs, static_cast<std::uint64_t>(index));
                resultId = dag.addEffectfulExpression(key);
            } else {
                if (isCommutative(instruction.op) && inputs[1] < inputs[0])
                    std::swap(inputs[0], inputs[1]);
                ExpressionKey key = instructionKey(instruction.op, inputs);
                const bool canReuse = instruction.effect == Effect::Pure &&
                                      instruction.dst.kind == OperandKind::Temp;
                if (canReuse) {
                    const std::optional<ExprId> existing =
                        dag.findEquivalentPure(key);
                    if (existing) {
                        resultId = *existing;
                        const auto reusable = reusableResults.find(resultId);
                        if (reusable != reusableResults.end()) {
                            instruction = makeInstruction(
                                Opcode::Copy, instruction.dst, reusable->second,
                                Operand::none(), instruction.line, instruction.column);
                            stats.changed = true;
                            ++stats.instructionsRewritten;
                        } else {
                            reusableResults.emplace(resultId, instruction.dst);
                        }
                    } else {
                        resultId = dag.addPureExpression(key);
                        reusableResults.emplace(resultId, instruction.dst);
                    }
                } else if (instruction.effect == Effect::Pure) {
                    resultId = dag.addPureExpression(key);
                } else {
                    resultId = dag.addEffectfulExpression(key);
                }
            }

            if (instruction.dst.kind == OperandKind::Temp)
                temps[instruction.dst.id] = resultId;
            else if (instruction.dst.kind == OperandKind::Symbol)
                symbols[instruction.dst.id] = resultId;
        }
        return stats;
    }
};

class DeadCodeEliminationPass final : public OptimizationPass {
public:
    PassKind kind() const noexcept override {
        return PassKind::DeadCodeElimination;
    }

    PassStats run(Program& program) const override {
        PassStats stats;
        stats.pass = kind();
        std::map<std::uint32_t, int> uses;
        for (const Instruction& instruction : program.code) {
            const int count = operandCount(instruction.op);
            if (count > 0 && instruction.a.kind == OperandKind::Temp)
                ++uses[instruction.a.id];
            if (count > 1 && instruction.b.kind == OperandKind::Temp)
                ++uses[instruction.b.id];
        }

        std::vector<bool> keep(program.code.size(), true);
        std::set<SymbolId> liveSymbols;
        for (std::size_t i = program.code.size(); i > 0; --i) {
            const Instruction& instruction = program.code[i - 1];
            const int count = operandCount(instruction.op);
            const bool deadTemp =
                instruction.effect == Effect::Pure &&
                instruction.dst.kind == OperandKind::Temp &&
                uses[instruction.dst.id] == 0;
            const bool deadSymbol =
                instruction.effect == Effect::Pure &&
                instruction.dst.kind == OperandKind::Symbol &&
                liveSymbols.count(instruction.dst.id) == 0;
            if (deadTemp || deadSymbol) {
                keep[i - 1] = false;
                ++stats.instructionsRemoved;
                if (count > 0 && instruction.a.kind == OperandKind::Temp)
                    --uses[instruction.a.id];
                if (count > 1 && instruction.b.kind == OperandKind::Temp)
                    --uses[instruction.b.id];
                continue;
            }

            if (instruction.dst.kind == OperandKind::Symbol)
                liveSymbols.erase(instruction.dst.id);
            if (count > 0 && instruction.a.kind == OperandKind::Symbol)
                liveSymbols.insert(instruction.a.id);
            if (count > 1 && instruction.b.kind == OperandKind::Symbol)
                liveSymbols.insert(instruction.b.id);
        }

        if (stats.instructionsRemoved > 0) {
            std::vector<Instruction> retained;
            retained.reserve(program.code.size() -
                             static_cast<std::size_t>(stats.instructionsRemoved));
            for (std::size_t i = 0; i < program.code.size(); ++i)
                if (keep[i]) retained.push_back(program.code[i]);
            program.code = std::move(retained);
            stats.changed = true;
        }
        return stats;
    }
};

class ExpressionBuilder {
public:
    void build(const Program& program) {
        for (std::size_t index = 0; index < program.code.size(); ++index) {
            const Instruction& instruction = program.code[index];
            const int count = operandCount(instruction.op);
            std::vector<ExprId> inputs;
            if (count > 0) {
                inputs.push_back(valueId(instruction.a));
                ++useCounts_[inputs.back()];
            }
            if (count > 1) {
                inputs.push_back(valueId(instruction.b));
                ++useCounts_[inputs.back()];
            }

            if (instruction.op == Opcode::Print) {
                dag_.addEffectfulExpression(instructionKey(
                    instruction.op, inputs, static_cast<std::uint64_t>(index)));
                continue;
            }

            ExprId resultId = 0;
            if (instruction.op == Opcode::Copy) {
                resultId = inputs.front();
            } else if (instruction.op == Opcode::Input) {
                resultId = dag_.addEffectfulExpression(instructionKey(
                    instruction.op, inputs, static_cast<std::uint64_t>(index)));
            } else {
                if (isCommutative(instruction.op) && inputs[1] < inputs[0])
                    std::swap(inputs[0], inputs[1]);
                const ExpressionKey key = instructionKey(instruction.op, inputs);
                resultId = instruction.effect == Effect::Pure
                               ? dag_.addPureExpression(key)
                               : dag_.addEffectfulExpression(key);
            }

            if (instruction.dst.kind == OperandKind::Temp)
                temps_[instruction.dst.id] = resultId;
            else if (instruction.dst.kind == OperandKind::Symbol)
                symbols_[instruction.dst.id] = resultId;
        }
    }

    const ExpressionDag& dag() const { return dag_; }
    const std::map<ExprId, int>& useCounts() const { return useCounts_; }

private:
    ExprId valueId(const Operand& operand) {
        if (operand.kind == OperandKind::Const) {
            const auto found = constants_.find(operand.imm);
            if (found != constants_.end()) return found->second;
            ExpressionKey key;
            key.op = OperatorKind::Constant;
            key.type = kInt32Type;
            key.attributes.push_back(static_cast<std::uint32_t>(operand.imm));
            const ExprId id = dag_.addPureExpression(key);
            constants_.emplace(operand.imm, id);
            return id;
        }
        if (operand.kind == OperandKind::Temp) return temps_.at(operand.id);
        if (operand.kind == OperandKind::Symbol) return symbols_.at(operand.id);
        throw std::logic_error("unexpected empty operand in optimizer snapshot");
    }

    ExpressionDag dag_;
    std::map<std::int32_t, ExprId> constants_;
    std::map<std::uint32_t, ExprId> temps_;
    std::map<SymbolId, ExprId> symbols_;
    std::map<ExprId, int> useCounts_;
};

std::string programState(const Program& program) {
    std::ostringstream state;
    state << program.tempCount << '|';
    for (const SymbolInfo& symbol : program.symbols)
        state << symbol.id << ':' << symbol.name.size() << ':' << symbol.name << ':'
              << static_cast<int>(symbol.type) << '|';
    for (const Instruction& instruction : program.code) {
        state << static_cast<int>(instruction.op) << ':'
              << static_cast<int>(instruction.effect) << ':'
              << static_cast<int>(instruction.type) << ':';
        const Operand operands[] = {instruction.dst, instruction.a, instruction.b};
        for (const Operand& operand : operands)
            state << static_cast<int>(operand.kind) << ',' << operand.imm << ','
                  << operand.id << ';';
        state << instruction.line << ',' << instruction.column << '|';
    }
    return state.str();
}

void buildSnapshots(OptimizationResult& result,
                    const available_sheep::IdentifierTrie* trie) {
    ExpressionBuilder builder;
    builder.build(result.optimizedIr);
    result.dagNodes = builder.dag().snapshot();

    AVLTree avl;
    for (const auto& node : result.dagNodes) {
        std::ostringstream key;
        key << 'e' << std::setw(20) << std::setfill('0') << node.id;
        const int useCount = builder.useCounts().count(node.id)
                                 ? builder.useCounts().at(node.id)
                                 : 0;
        ExprInfo info{static_cast<int>(node.id), key.str(), useCount};
        avl.insert(key.str(), info);
    }
    for (const std::string& key : avl.keysInOrder()) {
        const ExprInfo* info = avl.find(key);
        if (!info) continue;
        ExprMetadata metadata{info->dagNodeId, info->resultName, info->useCount};
        result.avlOrdered.push_back(metadata);
        result.avlNodes.push_back(
            AvlNodeSnapshot{key, metadata, avl.heightOfKey(key)});
    }
    for (const std::string& rotation : avl.rotationLog())
        result.avlRotations.push_back(RotationEvent{rotation});
    if (trie) result.trieEntries = trie->snapshot();
}

void reportFailure(OptimizationResult& result, const std::string& message) {
    Diagnostic diagnostic;
    diagnostic.severity = Severity::Error;
    diagnostic.stage = DiagStage::Optimization;
    diagnostic.message = message;
    result.diagnostics.push_back(std::move(diagnostic));
}

}  // namespace

std::unique_ptr<OptimizationPass> makeOptimizationPass(PassKind kind) {
    switch (kind) {
        case PassKind::ConstantFolding:
            return std::unique_ptr<OptimizationPass>(new ConstantFoldingPass());
        case PassKind::ConstantPropagation:
            return std::unique_ptr<OptimizationPass>(new ConstantPropagationPass());
        case PassKind::CommonSubexpressionElimination:
            return std::unique_ptr<OptimizationPass>(new CommonSubexpressionPass());
        case PassKind::DeadCodeElimination:
            return std::unique_ptr<OptimizationPass>(new DeadCodeEliminationPass());
    }
    return nullptr;
}

OptimizationResult optimizeProgram(const Program& initialIr,
                                   const PassOptions& options,
                                   const available_sheep::IdentifierTrie* trie) {
    OptimizationResult result;
    result.initialIr = initialIr;
    result.optimizedIr = initialIr;
    result.before = computeMetrics(initialIr);
    if (trie) result.trieEntries = trie->snapshot();

    const std::vector<std::string> inputErrors = validate(initialIr);
    if (!inputErrors.empty()) {
        reportFailure(result, "cannot optimize invalid initial IR: " + inputErrors.front());
        result.outcome = RunOutcome::OptimizationFailed;
        result.after = result.before;
        return result;
    }

    std::set<std::string> seenStates;
    seenStates.insert(programState(result.optimizedIr));
    const int limit = std::max(0, options.maxIterations);
    result.convergence = limit == 0 ? Convergence::IterationLimit
                                    : Convergence::Converged;

    constexpr PassKind order[] = {
        PassKind::ConstantFolding,
        PassKind::ConstantPropagation,
        PassKind::CommonSubexpressionElimination,
        PassKind::DeadCodeElimination};

    for (int iteration = 1; iteration <= limit; ++iteration) {
        IterationReport report;
        report.iteration = iteration;
        for (PassKind kind : order) {
            const int index = passIndex(kind);
            if (index < 0 || !options.enabled[index]) continue;
            std::unique_ptr<OptimizationPass> pass = makeOptimizationPass(kind);
            PassStats stats = pass->run(result.optimizedIr);
            report.changed = report.changed || stats.changed;
            report.passes.push_back(stats);

            const std::vector<std::string> errors = validate(result.optimizedIr);
            if (!errors.empty()) {
                reportFailure(result, "optimization produced invalid IR: " + errors.front());
                result.outcome = RunOutcome::OptimizationFailed;
                result.iterations.push_back(std::move(report));
                result.iterationsRun = iteration;
                result.after = computeMetrics(result.optimizedIr);
                return result;
            }
        }
        result.iterationsRun = iteration;
        result.iterations.push_back(report);
        if (!report.changed) {
            result.convergence = Convergence::Converged;
            break;
        }

        const std::string state = programState(result.optimizedIr);
        if (!seenStates.insert(state).second) {
            result.convergence = Convergence::RepeatedState;
            break;
        }
        if (iteration == limit) result.convergence = Convergence::IterationLimit;
    }

    result.outcome = RunOutcome::Optimized;
    result.after = computeMetrics(result.optimizedIr);
    buildSnapshots(result, trie);
    return result;
}
