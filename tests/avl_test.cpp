#include "Avltree.hpp"
#include <cassert>
#include <cstdio>
#include <map>
#include <random>
 
static ExprInfo makeInfo(int id) {
    ExprInfo e;
    e.dagNodeId = id;
    return e;
}
 
int main() {
    AVLTree t;
    assert(t.insert("b", makeInfo(1)));
    assert(!t.insert("b", makeInfo(2)));
    assert(t.find("b")->dagNodeId == 1);
    assert(t.erase("b"));
    assert(!t.erase("b"));
    assert(t.size() == 0);
 
    AVLTree r;
    r.insert("a", makeInfo(0));
    r.insert("b", makeInfo(0));
    r.insert("c", makeInfo(0));
    assert(r.rotationLog().size() == 1);
    assert(r.rotationLog()[0] == "Left at a");
    assert(r.height() == 2);
 
    std::mt19937 rng(42);
    AVLTree big;
    std::map<std::string, int> ref;
    for (int i = 0; i < 20000; i++) {
        std::string k = "k" + std::to_string(rng() % 2000);
        if (rng() % 3) {
            if (big.insert(k, makeInfo(i))) ref[k] = i;
        } else {
            assert(big.erase(k) == (ref.erase(k) > 0));
        }
    }
    assert(big.isValid());
    assert(big.size() == (int)ref.size());
    auto keys = big.keysInOrder();
    int i = 0;
    for (auto& kv : ref) assert(keys[i++] == kv.first);
 
    AVLTree seq;
    for (int n = 0; n < 100000; n++) seq.insert(std::to_string(1000000 + n), makeInfo(n));
    assert(seq.isValid());
    assert(seq.height() <= 24);
 
    std::puts("all tests passed");
}
 