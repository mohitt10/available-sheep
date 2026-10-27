#pragma once
#include <string>
#include <utility>
#include <vector>
 
struct ExprInfo {
    int dagNodeId = -1;
    std::string resultName;
    int useCount = 0;
};
 
struct AVLNode {
    std::string key;
    ExprInfo info;
    int height = 1;
    AVLNode* left = nullptr;
    AVLNode* right = nullptr;
    AVLNode(const std::string& k, const ExprInfo& i) : key(k), info(i) {}
};
 
class AVLTree {
public:
    AVLTree() = default;
    ~AVLTree() { destroy(root); }
    AVLTree(const AVLTree&) = delete;
    AVLTree& operator=(const AVLTree&) = delete;
 
    bool insert(const std::string& key, const ExprInfo& info);
    bool erase(const std::string& key);
    ExprInfo* find(const std::string& key);
    int heightOfKey(const std::string& key) const;
    bool contains(const std::string& key) { return find(key) != nullptr; }
 
    int size() const { return count; }
    int height() const { return heightOf(root); }
    std::vector<std::string> keysInOrder() const;
    const std::vector<std::string>& rotationLog() const { return log; }
    bool isValid() const;
 
private:
    AVLNode* root = nullptr;
    int count = 0;
    std::vector<std::string> log;
 
    static int heightOf(AVLNode* n) { return n ? n->height : 0; }
    static int balanceOf(AVLNode* n) { return n ? heightOf(n->left) - heightOf(n->right) : 0; }
    static void updateHeight(AVLNode* n);
    static void destroy(AVLNode* n);
 
    AVLNode* rotateRight(AVLNode* y);
    AVLNode* rotateLeft(AVLNode* x);
    AVLNode* rebalance(AVLNode* n);
    AVLNode* insertNode(AVLNode* n, const std::string& key, const ExprInfo& info, bool& added);
    AVLNode* eraseNode(AVLNode* n, const std::string& key, bool& removed);
};
 