# COMPILER OPTIMIZATION ENGINE

A local C++17/20 and Qt 6 application for compiling a restricted C-like
language, optimizing its intermediate representation, and visualizing the
engine's data structures. The [HLD](docs/Screenshot%20From%202026-10-21%2015-20-08.png)
defines the target architecture; the [final implementation plan](docs/final-implementation-plan.md)
tracks the complete application scope.

## Overview

The target application has four layers: Qt presentation, compiler frontend,
optimization engine, and data-structure engine. The repository currently
contains these Layer 4 C++ components:

- `expression_dag` — a DAG-backed expression registry that assigns stable IDs to expressions, tracks operand dependencies, reuses equivalent pure expressions, and exposes immutable snapshots.
- `avl_tree` — an AVL tree implementation for keyed metadata, with balancing and validation checks.
- `identifier_trie` — a Qt-free identifier index (case-sensitive) with exact lookup, prefix search, and edit-distance spelling suggestions; it stores spellings and their `SymbolId`s only.

The project is structured as a single CMake build with library targets and automated tests.

## Repository layout

- `include/expression_dag.hpp` — expression DAG public API and types.
- `src/expression_dag.cpp` — DAG node storage and graph logic.
- `include/Avltree.hpp` — AVL tree interface.
- `src/Avltree.cpp` — AVL balancing, insertion, deletion, and validation logic.
- `tests/expression_dag_test.cpp` — DAG behavior and correctness checks.
- `include/identifier_trie.hpp` / `src/identifier_trie.cpp` — identifier trie API and implementation.
- `include/shared_types.hpp` — shared `SymbolId` definition (the single alias for symbol IDs).
- `tests/identifier_trie_test.cpp` — trie behavior checks (including a brute-force comparison for suggestions).
- `tests/avl_test.cpp` — AVL tree insertion, balancing, and validation checks.
- `CMakeLists.txt` — workspace build configuration and CTest registration.

## Implementation status

CMake currently defines three library targets and three corresponding CTest
executables. The components have standalone implementations, but integration
is not yet complete: the AVL implementation is string-keyed rather than the
planned `ExprId` metadata index. Build and test status must be verified with a
native compiler; the last attempted configuration selected an ARM bare-metal
compiler and failed before project sources were compiled.

See the [final implementation plan](docs/final-implementation-plan.md) for
project scope, component contracts, completion criteria, and the recommended
work order. The [DAG](docs/implementation-plan-dag.md),
[AVL](docs/implementation-plan-avl.md), and
[trie](docs/implementation-plan-trie.md) documents contain component details.

## Build and test

From the project root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Example usage

The `ExpressionDag` API exposes operations such as:

- `addPureExpression(key)`
- `addEffectfulExpression(key)`
- `findEquivalentPure(key)`
- `operandsOf(id)`
- `usersOf(id)`
- `snapshot()`

The AVL tree API exposes:

- `insert(key, info)`
- `erase(key)`
- `find(key)`
- `keysInOrder()`
- `isValid()`

The `IdentifierTrie` API exposes:

- `insert(spelling, symbolId)`
- `contains(spelling)` / `findExact(spelling)`
- `findByPrefix(prefix, limit)`
- `suggest(spelling, limit, maxEditDistance = 2)`
- `snapshot()` (copied values for the GUI)

## Notes

- `ExpressionDag` treats purity as caller-defined; equivalent pure expressions are merged only when the caller chooses `addPureExpression`.
- Operand IDs must already exist in the graph, so the DAG stays acyclic and deterministic.
- The AVL tree validates structural balance and maintains a rotation log for debugging.
