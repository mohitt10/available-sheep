# Implementation Plan: Expression DAG

**Owner:** Person 2  
**HLD component:** Layer 4 — DAG  
**Reference:** [HLD image](<./Screenshot From 2026-10-21 15-20-08.png>)

**Target:** C++17/20. Keep Qt6 in the presentation/visualization layer, not in
the DAG API.

## Goal and boundaries

Implement the expression graph used by the optimization engine to represent
expression nodes and operand dependencies, discover common subexpressions, and
support common-subexpression-elimination (CSE) rewrites.

The DAG owns expression-node identity and dependency edges. It does not own the
parser's AST or perform the complete optimization pass loop. The optimizer
decides when rewrites are legal and applies the resulting replacements to its
IR.

## Proposed integration contract

Agree on shared `ExprId` and operator/type definitions with the other owners
before implementation. Suggested ID:

```cpp
using ExprId = std::uint64_t;
```

Suggested value-oriented interface:

```cpp
struct ExpressionKey {
    OperatorKind op;
    TypeId type;
    std::vector<ExprId> operands;
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
};
```

Names and exact types can be adjusted to match the eventual IR, but all APIs
shared with other components must use the same `ExprId`. `addPureExpression`
may reuse an equivalent node; `addEffectfulExpression` always creates a distinct
node. Operand order is significant unless the optimizer explicitly canonicalizes
a proven-commutative operation. Equivalence includes type and semantics.
`findEquivalentPure` searches only expressions eligible for sharing.

## Implementation steps

1. Agree on `ExprId`, operator/type representation, and expression-key semantics
   with the AVL owner and optimizer integrator.
2. Implement node storage and dependency edges. Keep edge direction explicit
   (operand-to-user or user-to-operand) and expose the same meaning consistently
   through the API.
3. Add a structural-key index so `addPureExpression` can reuse an equivalent
   node. Keep `addEffectfulExpression` out of that unique table.
4. Provide read-only node/edge snapshots for the visualizer instead of exposing
   mutable graph internals.
5. Add tests for node creation, repeated pure expressions, operand ordering,
   dependency/user queries, invalid IDs, and effectful-expression non-merging.
6. Integrate with the optimizer by returning an existing `ExprId` for a
   duplicate pure expression and letting the optimizer rewrite IR uses.

## Ownership and handoffs

- Own the DAG implementation, structural-key lookup, focused tests, and graph
  snapshot API.
- Optimizer integration: optimizer passes build/query the DAG and apply IR
  rewrites; this component does not mutate IR instructions.
- AVL integration: publish the same `ExprId` for every DAG node so the AVL
  index can attach and retrieve ordered metadata.
- GUI integration: provide immutable snapshots for graph display; visualization
  must not modify DAG nodes or dependencies.
- Coordinate ID and expression-key definitions before parallel implementation
  starts. Keep Qt types out of the DAG public API.

## Definition of done

- DAG node IDs and snapshot data use the shared expression types.
- Equivalent pure expressions reuse a node without incorrectly merging
  effectful expressions.
- Dependency queries and CSE-focused tests pass.
- The graph is readable by the optimizer and visualizer without either
  depending on its internal storage.
- Agree on source/header paths and register the focused tests with the project's
  build system once the source scaffold is created.
