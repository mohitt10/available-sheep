# Compiler Optimization Engine

A Qt-free C++17 compiler frontend and optimization engine with reusable
expression, identifier, and metadata indexes.

## Overview

The native pipeline accepts a restricted C-like language, builds and validates
an initial three-address IR, runs an optimization pipeline, and exposes copied
IR/data-structure snapshots and per-pass statistics.

- `frontend` — lexer, parser, semantic checks, and initial IR generation.
- `optimizer` — constant folding, constant propagation, common
  subexpression elimination, dead-code elimination, and fixed-point
  orchestration.
- `expression_dag` — typed expression keys, operand dependencies, reuse of
  equivalent pure expressions, and deterministic value snapshots.
- `avl_tree` — an `ExprId`-keyed metadata index with lookup/update/deletion,
  ordered snapshots, balance validation, and typed rotation events.
- `identifier_trie` — exact identifier lookup, prefix search, and spelling
  suggestions; scope resolution remains the frontend's responsibility.

The project is organized as CMake libraries with CTest coverage; no Qt
dependency is required for the compiler or optimizer libraries.

## Repository layout

- `include/expression_dag.hpp` — expression DAG public API and types.
- `src/expression_dag.cpp` — DAG node storage and graph logic.
- `include/Avltree.hpp` — AVL tree interface.
- `src/Avltree.cpp` — AVL balancing, insertion, deletion, and validation logic.
- `tests/expression_dag_test.cpp` — DAG behavior and correctness checks.
- `include/identifier_trie.hpp` / `src/identifier_trie.cpp` — identifier trie API and implementation.
- `include/shared_types.hpp` — canonical `SymbolId`, `ExprId`, and `TypeId`
  definitions.
- `include/Optimizer.hpp` / `src/Optimizer.cpp` — pass interface,
  transformations, orchestration, and visualization snapshots.
- `tests/OptimizerTests.cpp` — pass semantics, fixed-point behavior, limits,
  and snapshot tests.
- `tests/identifier_trie_test.cpp` — trie behavior checks (including a brute-force comparison for suggestions).
- `tests/avl_test.cpp` — AVL tree insertion, balancing, and validation checks.
- `CMakeLists.txt` — workspace build configuration and CTest registration.

## Integration status

All components and tests are wired through CMake. Optimization results include
the initial and optimized IR, before/after metrics, pass-level reports,
convergence status, and copied DAG/AVL/trie snapshots.

## Build and test

From the project root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Example usage

The `ExpressionDag` API exposes:

- `addPureExpression(key)`
- `addEffectfulExpression(key)`
- `findEquivalentPure(key)`
- `operandsOf(id)`
- `usersOf(id)`
- `snapshot()`

The optimizer API exposes:

- `makeOptimizationPass(kind)` for individual passes.
- `optimizeProgram(initialIr, options, trie)` for the ordered fixed-point run.

The AVL tree API is keyed by shared `ExprId` values and provides:

- `insert(id, info)` / `update(id, info)` / `erase(id)` / `find(id)`.
- `snapshot()` in ascending `ExprId` order.
- `rotationEvents()` with typed single/double rotation records.

The `IdentifierTrie` API exposes:

- `insert(spelling, symbolId)`, `findExact(spelling)`, and `contains(spelling)`.
- `findByPrefix(prefix, limit)` and `suggest(spelling, limit, distance)`.
- `snapshot()` for a copied, lexicographically ordered view.

## Semantics

- The optimizer runs constant folding, constant propagation, CSE, and DCE in
  that order until no pass changes the IR or the iteration limit is reached.
- Operations that may trap are not removed or merged unsafely; observable
  input/output effects are preserved.
- DAG operands must already exist, keeping expression dependencies acyclic.
- Shared IDs are defined once in `shared_types.hpp`.
