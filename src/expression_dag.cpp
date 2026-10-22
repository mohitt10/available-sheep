#include "expression_dag.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace available_sheep {
namespace {

void hashCombine(std::size_t& seed, std::size_t value) noexcept {
    seed ^= value + static_cast<std::size_t>(0x9e3779b9U) + (seed << 6U) +
            (seed >> 2U);
}

}  // namespace

bool ExpressionKey::operator==(const ExpressionKey& other) const noexcept {
    return op == other.op && type == other.type && operands == other.operands &&
           attributes == other.attributes;
}

bool ExpressionNodeSnapshot::operator==(
    const ExpressionNodeSnapshot& other) const noexcept {
    return id == other.id && key == other.key && isPure == other.isPure;
}

std::size_t ExpressionDag::ExpressionKeyHash::operator()(
    const ExpressionKey& key) const noexcept {
    std::size_t seed = 0;
    hashCombine(seed, std::hash<std::uint16_t>{}(
                          static_cast<std::uint16_t>(key.op)));
    hashCombine(seed, std::hash<TypeId>{}(key.type));
    for (const ExprId operand : key.operands) {
        hashCombine(seed, std::hash<ExprId>{}(operand));
    }
    hashCombine(seed, key.operands.size());
    for (const std::uint64_t attribute : key.attributes) {
        hashCombine(seed, std::hash<std::uint64_t>{}(attribute));
    }
    hashCombine(seed, key.attributes.size());
    return seed;
}

ExprId ExpressionDag::addPureExpression(const ExpressionKey& key) {
    validateOperands(key);
    const auto existing = pureExpressions_.find(key);
    if (existing != pureExpressions_.end()) {
        return existing->second;
    }
    return addNode(key, true);
}

ExprId ExpressionDag::addEffectfulExpression(const ExpressionKey& key) {
    validateOperands(key);
    return addNode(key, false);
}

std::optional<ExprId> ExpressionDag::findEquivalentPure(
    const ExpressionKey& key) const {
    validateOperands(key);
    const auto existing = pureExpressions_.find(key);
    if (existing == pureExpressions_.end()) {
        return std::nullopt;
    }
    return existing->second;
}

std::vector<ExprId> ExpressionDag::operandsOf(ExprId expression) const {
    return nodeFor(expression).key.operands;
}

std::vector<ExprId> ExpressionDag::usersOf(ExprId expression) const {
    nodeFor(expression);
    const auto users = users_.find(expression);
    if (users == users_.end()) {
        return {};
    }
    return {users->second.begin(), users->second.end()};
}

std::vector<ExpressionNodeSnapshot> ExpressionDag::snapshot() const {
    std::vector<ExpressionNodeSnapshot> result;
    result.reserve(nodes_.size());
    for (const auto& [id, node] : nodes_) {
        result.push_back({id, node.key, node.isPure});
    }
    std::sort(result.begin(), result.end(),
              [](const ExpressionNodeSnapshot& left,
                 const ExpressionNodeSnapshot& right) {
                  return left.id < right.id;
              });
    return result;
}

std::size_t ExpressionDag::size() const noexcept {
    return nodes_.size();
}

void ExpressionDag::validateOperands(const ExpressionKey& key) const {
    for (const ExprId operand : key.operands) {
        if (nodes_.find(operand) == nodes_.end()) {
            throw std::invalid_argument("expression operand does not exist");
        }
    }
}

ExprId ExpressionDag::addNode(const ExpressionKey& key, bool isPure) {
    if (nextId_ == 0) {
        throw std::overflow_error("expression ID space exhausted");
    }

    const ExprId id = nextId_;
    const auto inserted = nodes_.emplace(id, Node{key, isPure});
    if (!inserted.second) {
        throw std::logic_error("expression ID is already in use");
    }

    bool pureIndexInserted = false;
    try {
        for (const ExprId operand : key.operands) {
            users_[operand].insert(id);
        }
        if (isPure) {
            const auto indexed = pureExpressions_.emplace(key, id);
            if (!indexed.second) {
                throw std::logic_error("pure expression key is already indexed");
            }
            pureIndexInserted = true;
        }
    } catch (...) {
        if (pureIndexInserted) {
            pureExpressions_.erase(key);
        }
        for (const ExprId operand : key.operands) {
            const auto users = users_.find(operand);
            if (users != users_.end()) {
                users->second.erase(id);
                if (users->second.empty()) {
                    users_.erase(users);
                }
            }
        }
        nodes_.erase(id);
        throw;
    }

    nextId_ = id == std::numeric_limits<ExprId>::max() ? 0 : id + 1;
    return id;
}

const ExpressionDag::Node& ExpressionDag::nodeFor(ExprId expression) const {
    const auto found = nodes_.find(expression);
    if (found == nodes_.end()) {
        throw std::out_of_range("expression ID does not exist");
    }
    return found->second;
}

}  // namespace available_sheep
