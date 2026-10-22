#include "Avltree.hpp"
#include <algorithm>
#include <cstdlib>
 
void AVLTree::updateHeight(AVLNode* n) {
    n->height = 1 + std::max(heightOf(n->left), heightOf(n->right));
}
 
void AVLTree::destroy(AVLNode* n) {
    if (!n) return;
    destroy(n->left);
    destroy(n->right);
    delete n;
}
 
AVLNode* AVLTree::rotateRight(AVLNode* y) {
    AVLNode* x = y->left;
    y->left = x->right;
    x->right = y;
    updateHeight(y);
    updateHeight(x);
    return x;
}
 
AVLNode* AVLTree::rotateLeft(AVLNode* x) {
    AVLNode* y = x->right;
    x->right = y->left;
    y->left = x;
    updateHeight(x);
    updateHeight(y);
    return y;
}
 
AVLNode* AVLTree::rebalance(AVLNode* n) {
    updateHeight(n);
    int balance = balanceOf(n);
 
    if (balance > 1) {
        if (balanceOf(n->left) < 0) {
            log.push_back("LeftRight at " + n->key);
            n->left = rotateLeft(n->left);
        } else {
            log.push_back("Right at " + n->key);
        }
        return rotateRight(n);
    }
 
    if (balance < -1) {
        if (balanceOf(n->right) > 0) {
            log.push_back("RightLeft at " + n->key);
            n->right = rotateRight(n->right);
        } else {
            log.push_back("Left at " + n->key);
        }
        return rotateLeft(n);
    }
 
    return n;
}
 
AVLNode* AVLTree::insertNode(AVLNode* n, const std::string& key, const ExprInfo& info, bool& added) {
    if (!n) {
        added = true;
        return new AVLNode(key, info);
    }
    if (key < n->key)
        n->left = insertNode(n->left, key, info, added);
    else if (key > n->key)
        n->right = insertNode(n->right, key, info, added);
    else
        return n;
    return rebalance(n);
}
 
AVLNode* AVLTree::eraseNode(AVLNode* n, const std::string& key, bool& removed) {
    if (!n) return nullptr;
 
    if (key < n->key) {
        n->left = eraseNode(n->left, key, removed);
    } else if (key > n->key) {
        n->right = eraseNode(n->right, key, removed);
    } else {
        removed = true;
        if (!n->left || !n->right) {
            AVLNode* child = n->left ? n->left : n->right;
            delete n;
            return child;
        }
        AVLNode* next = n->right;
        while (next->left) next = next->left;
        n->key = next->key;
        n->info = next->info;
        bool ignored = false;
        n->right = eraseNode(n->right, n->key, ignored);
    }
    return rebalance(n);
}
 
bool AVLTree::insert(const std::string& key, const ExprInfo& info) {
    bool added = false;
    root = insertNode(root, key, info, added);
    if (added) count++;
    return added;
}
 
bool AVLTree::erase(const std::string& key) {
    bool removed = false;
    root = eraseNode(root, key, removed);
    if (removed) count--;
    return removed;
}
 
ExprInfo* AVLTree::find(const std::string& key) {
    AVLNode* n = root;
    while (n) {
        if (key == n->key) return &n->info;
        n = (key < n->key) ? n->left : n->right;
    }
    return nullptr;
}
 
static void collectKeys(AVLNode* n, std::vector<std::string>& out) {
    if (!n) return;
    collectKeys(n->left, out);
    out.push_back(n->key);
    collectKeys(n->right, out);
}
 
std::vector<std::string> AVLTree::keysInOrder() const {
    std::vector<std::string> out;
    collectKeys(root, out);
    return out;
}
 
static bool checkNode(AVLNode* n) {
    if (!n) return true;
    int hl = n->left ? n->left->height : 0;
    int hr = n->right ? n->right->height : 0;
    if (std::abs(hl - hr) > 1) return false;
    if (n->height != 1 + std::max(hl, hr)) return false;
    if (n->left && !(n->left->key < n->key)) return false;
    if (n->right && !(n->key < n->right->key)) return false;
    return checkNode(n->left) && checkNode(n->right);
}
 
bool AVLTree::isValid() const { return checkNode(root); }