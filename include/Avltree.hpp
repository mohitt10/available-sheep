#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "shared_types.hpp"

struct ExprInfo {
    std::string resultName;
    std::size_t useCount = 0;
};

enum class AvlRotationKind : std::uint8_t {
    Left,
    Right,
    LeftRight,
    RightLeft
};

struct AvlRotationEvent {
    AvlRotationKind kind = AvlRotationKind::Left;
    available_sheep::ExprId pivot{};
};

struct AvlEntrySnapshot {
    available_sheep::ExprId id{};
    ExprInfo info;
    int height = 1;
    std::optional<available_sheep::ExprId> leftChild;
    std::optional<available_sheep::ExprId> rightChild;
};

struct AVLNode {
    available_sheep::ExprId id;
    ExprInfo info;
    int height = 1;
    AVLNode* left = nullptr;
    AVLNode* right = nullptr;

    AVLNode(available_sheep::ExprId key, const ExprInfo& value)
        : id(key), info(value) {}
};

class AVLTree {
public:
    AVLTree() = default;
    ~AVLTree() { destroy(root); }
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;

    bool insert(available_sheep::ExprId id, const ExprInfo& info);
    bool update(available_sheep::ExprId id, const ExprInfo& info);
    bool erase(available_sheep::ExprId id);
    ExprInfo* find(available_sheep::ExprId id);
    const ExprInfo* find(available_sheep::ExprId id) const;
    bool contains(available_sheep::ExprId id) const;

    std::size_t size() const noexcept { return count; }
    int height() const noexcept { return heightOf(root); }
    std::vector<AvlEntrySnapshot> snapshot() const;
    const std::vector<AvlRotationEvent>& rotationEvents() const noexcept {
        return rotations;
    }
    bool isValid() const;

private:
    AVLNode* root = nullptr;
    std::size_t count = 0;
    std::vector<AvlRotationEvent> rotations;

    static int heightOf(const AVLNode* node) {
        return node ? node->height : 0;
    }
    static int balanceOf(const AVLNode* node) {
        return node ? heightOf(node->left) - heightOf(node->right) : 0;
    }
    static void updateHeight(AVLNode* node);
    static void destroy(AVLNode* node);

    AVLNode* rotateRight(AVLNode* node);
    AVLNode* rotateLeft(AVLNode* node);
    AVLNode* rebalance(AVLNode* node);
    AVLNode* insertNode(AVLNode* node, available_sheep::ExprId id,
                        const ExprInfo& info, bool& added);
    AVLNode* eraseNode(AVLNode* node, available_sheep::ExprId id,
                       bool& removed);
};
