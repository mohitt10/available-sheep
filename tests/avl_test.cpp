#include "Avltree.hpp"

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
    AVLTree tree;
    assert(tree.insert(2, makeInfo(1)));
    assert(!tree.insert(2, makeInfo(2)));
    assert(tree.find(2)->useCount == 1);
    assert(tree.update(2, makeInfo(3)));
    assert(tree.find(2)->useCount == 3);
    assert(!tree.update(3, makeInfo(4)));
    assert(tree.erase(2));
    assert(!tree.erase(2));
    assert(tree.size() == 0);

    AVLTree rightRotation;
    rightRotation.insert(1, makeInfo(0));
    rightRotation.insert(2, makeInfo(0));
    rightRotation.insert(3, makeInfo(0));
    assert(rightRotation.rotationEvents().size() == 1);
    assert(rightRotation.rotationEvents()[0].kind == AvlRotationKind::Left);
    assert(rightRotation.rotationEvents()[0].pivot == 1);
    assert(rightRotation.height() == 2);
    const auto rotatedSnapshot = rightRotation.snapshot();
    assert(rotatedSnapshot.size() == 3);
    assert(rotatedSnapshot[0].id == 1);
    assert(!rotatedSnapshot[0].leftChild && !rotatedSnapshot[0].rightChild);
    assert(rotatedSnapshot[1].id == 2);
    assert(rotatedSnapshot[1].leftChild == 1);
    assert(rotatedSnapshot[1].rightChild == 3);
    assert(rotatedSnapshot[2].id == 3);

    const AvlRotationKind rotationKinds[] = {
        AvlRotationKind::Right, AvlRotationKind::Left,
        AvlRotationKind::LeftRight, AvlRotationKind::RightLeft};
    const ExprId insertionOrders[][3] = {
        {3, 2, 1}, {1, 2, 3}, {3, 1, 2}, {1, 3, 2}};
    for (std::size_t rotation = 0; rotation < 4; ++rotation) {
        AVLTree rotated;
        for (ExprId key : insertionOrders[rotation])
            rotated.insert(key, makeInfo(0));
        assert(rotated.rotationEvents().size() == 1);
        assert(rotated.rotationEvents().front().kind == rotationKinds[rotation]);
        assert(rotated.isValid());
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
    std::map<ExprId, std::size_t> reference;
    for (int i = 0; i < 20000; ++i) {
        const ExprId key = rng() % 2000;
        if (rng() % 3) {
            if (big.insert(key, makeInfo(static_cast<std::size_t>(i))))
                reference[key] = static_cast<std::size_t>(i);
        } else {
            assert(big.erase(key) == (reference.erase(key) > 0));
        }
    }
    assert(big.isValid());
    assert(big.size() == reference.size());
    const auto entries = big.snapshot();
    assert(entries.size() == reference.size());
    std::size_t index = 0;
    for (const auto& entry : reference) {
        assert(entries[index].id == entry.first);
        assert(entries[index].info.useCount == entry.second);
        ++index;
    }

    AVLTree sequence;
    for (ExprId id = 1; id <= 100000; ++id)
        sequence.insert(id, makeInfo(static_cast<std::size_t>(id)));
    assert(sequence.isValid());
    assert(sequence.height() <= 24);

    std::puts("all tests passed");
}
