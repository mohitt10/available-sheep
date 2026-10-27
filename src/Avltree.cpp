#include "Avltree.hpp"

#include <algorithm>
#include <optional>

namespace {

void collectSnapshots(const AVLNode* node, std::vector<AvlEntrySnapshot>& result) {
    if (!node) return;
    collectSnapshots(node->left, result);
    result.push_back({node->id, node->info, node->height});
    collectSnapshots(node->right, result);
}

int validateNode(const AVLNode* node, std::optional<available_sheep::ExprId> lower,
                 std::optional<available_sheep::ExprId> upper,
                 std::size_t& count, bool& valid) {
    if (!node) return 0;
    if ((lower && node->id <= *lower) || (upper && node->id >= *upper))
        valid = false;

    const int leftHeight =
        validateNode(node->left, lower, node->id, count, valid);
    const int rightHeight =
        validateNode(node->right, node->id, upper, count, valid);
    ++count;
    const int expectedHeight = 1 + std::max(leftHeight, rightHeight);
    if (node->height != expectedHeight ||
        std::abs(leftHeight - rightHeight) > 1)
        valid = false;
    return expectedHeight;
}

}  // namespace

void AVLTree::updateHeight(AVLNode* node) {
    node->height = 1 + std::max(heightOf(node->left), heightOf(node->right));
}

void AVLTree::destroy(AVLNode* node) {
    if (!node) return;
    destroy(node->left);
    destroy(node->right);
    delete node;
}

AVLNode* AVLTree::rotateRight(AVLNode* rootNode) {
    AVLNode* newRoot = rootNode->left;
    rootNode->left = newRoot->right;
    newRoot->right = rootNode;
    updateHeight(rootNode);
    updateHeight(newRoot);
    return newRoot;
}

AVLNode* AVLTree::rotateLeft(AVLNode* rootNode) {
    AVLNode* newRoot = rootNode->right;
    rootNode->right = newRoot->left;
    newRoot->left = rootNode;
    updateHeight(rootNode);
    updateHeight(newRoot);
    return newRoot;
}

AVLNode* AVLTree::rebalance(AVLNode* node) {
    if (!node) return nullptr;
    updateHeight(node);
    const int balance = balanceOf(node);

    if (balance > 1) {
        if (balanceOf(node->left) < 0) {
            rotations.push_back({AvlRotationKind::LeftRight, node->id});
            node->left = rotateLeft(node->left);
        } else {
            rotations.push_back({AvlRotationKind::Right, node->id});
        }
        return rotateRight(node);
    }

    if (balance < -1) {
        if (balanceOf(node->right) > 0) {
            rotations.push_back({AvlRotationKind::RightLeft, node->id});
            node->right = rotateRight(node->right);
        } else {
            rotations.push_back({AvlRotationKind::Left, node->id});
        }
        return rotateLeft(node);
    }
    return node;
}

AVLNode* AVLTree::insertNode(AVLNode* node, available_sheep::ExprId id,
                             const ExprInfo& info, bool& added) {
    if (!node) {
        added = true;
        return new AVLNode(id, info);
    }
    if (id < node->id)
        node->left = insertNode(node->left, id, info, added);
    else if (id > node->id)
        node->right = insertNode(node->right, id, info, added);
    else
        return node;
    return rebalance(node);
}

AVLNode* AVLTree::eraseNode(AVLNode* node, available_sheep::ExprId id,
                            bool& removed) {
    if (!node) return nullptr;

    if (id < node->id) {
        node->left = eraseNode(node->left, id, removed);
    } else if (id > node->id) {
        node->right = eraseNode(node->right, id, removed);
    } else {
        removed = true;
        if (!node->left || !node->right) {
            AVLNode* child = node->left ? node->left : node->right;
            delete node;
            return child;
        }
        AVLNode* successor = node->right;
        while (successor->left) successor = successor->left;
        node->id = successor->id;
        node->info = successor->info;
        bool ignored = false;
        node->right = eraseNode(node->right, successor->id, ignored);
    }
    return rebalance(node);
}

bool AVLTree::insert(available_sheep::ExprId id, const ExprInfo& info) {
    bool added = false;
    root = insertNode(root, id, info, added);
    if (added) ++count;
    return added;
}

bool AVLTree::update(available_sheep::ExprId id, const ExprInfo& info) {
    AVLNode* node = root;
    while (node) {
        if (id == node->id) {
            node->info = info;
            return true;
        }
        node = id < node->id ? node->left : node->right;
    }
    return false;
}

bool AVLTree::erase(available_sheep::ExprId id) {
    bool removed = false;
    root = eraseNode(root, id, removed);
    if (removed) --count;
    return removed;
}

ExprInfo* AVLTree::find(available_sheep::ExprId id) {
    return const_cast<ExprInfo*>(
        static_cast<const AVLTree&>(*this).find(id));
}

const ExprInfo* AVLTree::find(available_sheep::ExprId id) const {
    const AVLNode* node = root;
    while (node) {
        if (id == node->id) return &node->info;
        node = id < node->id ? node->left : node->right;
    }
    return nullptr;
}

bool AVLTree::contains(available_sheep::ExprId id) const {
    return find(id) != nullptr;
}

std::vector<AvlEntrySnapshot> AVLTree::snapshot() const {
    std::vector<AvlEntrySnapshot> result;
    result.reserve(count);
    collectSnapshots(root, result);
    return result;
}

bool AVLTree::isValid() const {
    std::size_t validatedCount = 0;
    bool valid = true;
    validateNode(root, std::nullopt, std::nullopt, validatedCount, valid);
    return valid && validatedCount == count;
}
