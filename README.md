# Expression DAG Branch

## Purpose

This branch adds the expression DAG component for the compiler optimization
engine. The DAG assigns expression IDs, records operand dependencies, reuses
equivalent pure expressions, and exposes copied snapshots for consumers such
as the optimizer, AVL index, and visualizer.

The DAG does not parse source code, own the compiler IR, or perform optimizer
passes. In particular, the optimizer remains responsible for rewriting IR uses
when a pure expression is reused.

## Files added

- `include/available_sheep/expression_dag.hpp` — public types and API.
- `src/expression_dag.cpp` — node storage, pure-expression indexing, and graph
  queries.
- `tests/expression_dag_test.cpp` — focused behavior tests.
- `CMakeLists.txt` — C++17 library and CTest setup.

## API overview

The public API is in the `available_sheep` namespace:

- `addPureExpression(key)` returns the existing ID for an equivalent pure
  expression, or creates a new node.
- `addEffectfulExpression(key)` always creates a new node and never inserts it
  into the pure-expression index.
- `findEquivalentPure(key)` finds a matching pure node, if one exists.
- `operandsOf(id)` returns the node's ordered operand IDs.
- `usersOf(id)` returns distinct user IDs in ascending order.
- `snapshot()` returns copied node data sorted by ID.
- `size()` returns the number of stored nodes.

`ExprId` is a 64-bit unsigned integer. IDs begin at `1`; `0` is reserved as
invalid. `TypeId` is currently a 32-bit unsigned integer. `OperatorKind` and
these aliases are provisional component definitions and should be reconciled
with shared compiler/IR types before other components depend on them.

## Expression identity and graph behavior

An `ExpressionKey` contains the operator, result type, ordered operand IDs, and
a vector of integer semantic attributes. All of these fields participate in
equality and hashing. The hash is only used to locate candidates; full key
equality determines whether expressions match.

Operand order and duplicate operands are preserved. For example, a node with
operands `{x, x}` reports both entries from `operandsOf(x)` (more precisely,
`operandsOf(node)`), while `usersOf(x)` lists that node once. The DAG does not
canonicalize commutative operations.

Operands must refer to nodes that already exist, which ensures newly created
edges point to earlier nodes and the graph remains acyclic. Adding a node with
an unknown operand throws `std::invalid_argument`; querying an unknown node ID
throws `std::out_of_range`.

Purity is selected by the caller using the insertion method. The component
cannot infer whether an operation is safe to share. Callers must only use
`addPureExpression` when reusing the expression is semantically valid, and
must include every meaning-affecting value in `attributes`.

## Build and test

From the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The focused tests cover ID creation, pure-expression reuse, key-field
differences, effectful-expression non-merging, operand/user queries, invalid
IDs, and deterministic copied snapshots.

## Integration notes

- Coordinate `ExprId`, `TypeId`, operator definitions, and expression
  attributes with the optimizer and AVL-index owners before treating these
  component types as shared contracts.
- The optimizer can use the ID returned for a duplicate pure expression to
  rewrite IR uses; IR mutation is outside this component.
- The AVL index should use the same `ExprId` and should not duplicate DAG
  dependencies.
- The visualizer should consume `snapshot()` data and must not access mutable
  graph internals.
- Qt types are intentionally absent from the DAG API.
