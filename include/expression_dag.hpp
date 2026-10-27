#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <set>
#include <unordered_map>
#include <vector>

namespace available_sheep {

using ExprId = std::uint64_t;
using TypeId = std::uint32_t;

enum class OperatorKind : std::uint16_t {
    Constant,
    Identifier,
    Add,
    Subtract,
    Multiply,
    Divide,
    Remainder,
    Negate,
    LogicalNot,
    BitwiseNot,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LogicalAnd,
    LogicalOr,
    BitwiseAnd,
    BitwiseOr,
    BitwiseXor,
    ShiftLeft,
    ShiftRight,
    Cast,
    Call,
    Load,
    Store,
    Assign,
    Select,
    Other
};

struct ExpressionKey {
    OperatorKind op{};
    TypeId type{};
    std::vector<ExprId> operands;
    std::vector<std::uint64_t> attributes;

    bool operator==(const ExpressionKey& other) const noexcept;
};

struct ExpressionNodeSnapshot {
    ExprId id{};
    ExpressionKey key;
    bool isPure{};

    bool operator==(const ExpressionNodeSnapshot& other) const noexcept;
};

class ExpressionDag {
public:
    ExprId addPureExpression(const ExpressionKey& key);
    ExprId addEffectfulExpression(const ExpressionKey& key);

    std::optional<ExprId> findEquivalentPure(
        const ExpressionKey& key) const;
    std::vector<ExprId> operandsOf(ExprId expression) const;
    std::vector<ExprId> usersOf(ExprId expression) const;
    std::vector<ExpressionNodeSnapshot> snapshot() const;

    std::size_t size() const noexcept;

private:
    struct ExpressionKeyHash {
        std::size_t operator()(const ExpressionKey& key) const noexcept;
    };

    struct Node {
        ExpressionKey key;
        bool isPure;
    };

    void validateOperands(const ExpressionKey& key) const;
    ExprId addNode(const ExpressionKey& key, bool isPure);
    const Node& nodeFor(ExprId expression) const;

    ExprId nextId_{1};
    std::unordered_map<ExprId, Node> nodes_;
    std::unordered_map<ExpressionKey, ExprId, ExpressionKeyHash> pureExpressions_;
    std::unordered_map<ExprId, std::set<ExprId>> users_;
};

}  // namespace available_sheep
