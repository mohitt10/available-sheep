# Implementation Plan: AVL Expression Index

**Owner:** Person 3  
**HLD component:** Layer 4 — AVL tree  
**Reference:** [HLD image](<./Screenshot From 2026-10-21 15-20-08.png>)

**Target:** C++17/20. Keep Qt6 in the presentation/visualization layer, not in
the AVL API.

## Goal and boundaries

Implement the ordered index for expression metadata used by the optimizer and
data-structure visualizer. It provides efficient lookup and ordered traversal,
and exposes enough information to visualize rotations.

The AVL tree indexes metadata by the shared expression ID. The DAG remains the
owner of expression structure and dependencies; the AVL tree must not duplicate
the graph or decide CSE equivalence.

## Proposed integration contract

Use the shared `ExprId` definition from the DAG/IR integration contract:

```cpp
using ExprId = std::uint64_t;
```

Suggested API:

```cpp
class ExpressionAvlIndex {
public:
    void insertOrAssign(ExprId expression, ExpressionMetadata metadata);
    const ExpressionMetadata* find(ExprId expression) const;
    bool erase(ExprId expression);
    std::vector<ExpressionEntrySnapshot> inOrderSnapshot() const;
    std::vector<RotationEvent> takeRotationEvents();
};
```

The concrete metadata fields should be agreed with the optimizer and DAG owner.
Keep the key ordering based on `ExprId`; do not use a pointer address or a
mutable expression payload as the key. Snapshots and rotation events must be
plain value types and must not expose tree-node pointers.

## Implementation steps

1. Agree on `ExprId` and the minimal `ExpressionMetadata` fields needed by
   optimization and visualization. Keep metadata owned by this index only if
   there is no canonical IR/DAG owner for those fields.
2. Implement AVL insertion, lookup, update, and deletion with height maintenance
   and single/double rotations.
3. Add a validator usable in tests that checks ordering, stored heights, and the
   AVL balance bound after mutations.
4. Record rotation events as value data (rotation kind and affected expression
   IDs) for the visualizer. Do not make visualization a dependency of balancing.
5. Provide in-order snapshots for deterministic display and diagnostics.
6. Add tests for empty/single-node trees, ordered and reverse-ordered inserts,
   all rotation cases, updates, deletion/rebalancing, missing keys, and snapshot
   ordering.

## Ownership and handoffs

- Own the AVL implementation, balancing logic, metadata index, rotation event
  values, and focused tests.
- DAG integration: index each DAG node by its shared `ExprId`; obtain graph
  dependencies from the DAG, not from this tree.
- Optimizer integration: update metadata when a pass changes relevant
  expression facts; do not treat the AVL tree as the source of IR truth.
- GUI integration: consume in-order snapshots and rotation events through
  read-only value types; the GUI must not inspect AVL nodes.
- Coordinate shared types and metadata fields before parallel implementation
  starts. Keep Qt types out of the AVL public API.

## Definition of done

- Lookup, updates, deletion, and traversal preserve BST ordering and AVL
  balance, verified by invariant tests.
- Rotation events are deterministic value snapshots and do not affect tree
  correctness.
- The index uses the shared `ExprId` and does not duplicate DAG structure.
- Agree on source/header paths and register the focused tests with the project's
  build system once the source scaffold is created.
