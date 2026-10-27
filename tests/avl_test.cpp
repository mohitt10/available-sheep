#include "Avltree.hpp"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <limits>
#include <map>
#include <random>

using available_sheep::ExprId;

static ExprInfo makeInfo(std::size_t uses) {
    return {"result", uses};
}

int main() {
    AVLTree t;
    assert(t.insert(2, makeInfo(1)));
    assert(!t.insert(2, makeInfo(2)));
    assert(t.find(2)->useCount == 1);
    assert(t.update(2, makeInfo(3)));
    assert(t.find(2)->useCount == 3);
    assert(!t.update(3, makeInfo(4)));
    assert(t.erase(2));
    assert(!t.erase(2));
    assert(t.size() == 0);

    AVLTree r;
    r.insert(1, makeInfo(0));
    r.insert(2, makeInfo(0));
    r.insert(3, makeInfo(0));
    assert(r.rotationEvents().size() == 1);
    assert(r.rotationEvents()[0].kind == AvlRotationKind::Left);
    assert(r.rotationEvents()[0].pivot == 1);
    assert(r.height() == 2);

    const AvlRotationKind rotationKinds[] = {
        AvlRotationKind::Right,
        AvlRotationKind::Left,
        AvlRotationKind::LeftRight,
        AvlRotationKind::RightLeft};
    const ExprId insertionOrders[][3] = {
        {3, 2, 1}, {1, 2, 3}, {3, 1, 2}, {1, 3, 2}};
    for (std::size_t rotation = 0; rotation < 4; ++rotation) {
        AVLTree tree;
        for (ExprId key : insertionOrders[rotation])
            tree.insert(key, makeInfo(0));
        assert(tree.rotationEvents().size() == 1);
        assert(tree.rotationEvents().front().kind == rotationKinds[rotation]);
        assert(tree.isValid());
    }

    AVLTree wideIds;
    const ExprId largeId = std::numeric_limits<ExprId>::max();
    assert(wideIds.insert(largeId, makeInfo(5)));
    const auto copiedSnapshot = wideIds.snapshot();
    assert(copiedSnapshot.front().id == largeId);
    assert(copiedSnapshot.front().info.useCount == 5);
    assert(wideIds.update(largeId, makeInfo(6)));
    assert(copiedSnapshot.front().info.useCount == 5);

    std::mt19937 rng(42);
    AVLTree big;
    std::map<ExprId, std::size_t> ref;
    for (int i = 0; i < 20000; i++) {
        const ExprId key = rng() % 2000;
        if (rng() % 3) {
            if (big.insert(key, makeInfo(i))) ref[key] = i;
        } else {
            assert(big.erase(key) == (ref.erase(key) > 0));
        }
    }
    assert(big.isValid());
    assert(big.size() == ref.size());
    const auto entries = big.snapshot();
    assert(entries.size() == ref.size());
    std::size_t i = 0;
    for (const auto& entry : ref) {
        assert(entries[i].id == entry.first);
        assert(entries[i].info.useCount == entry.second);
        ++i;
    }

    AVLTree seq;
    for (ExprId n = 1; n <= 100000; ++n)
        seq.insert(n, makeInfo(static_cast<std::size_t>(n)));
    assert(seq.isValid());
    assert(seq.height() <= 24);

    std::puts("all tests passed");
}
 