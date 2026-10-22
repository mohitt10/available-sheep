# COMPILER OPTIMIZATION ENGINE

A small C++ project for expression-graph and AVL-tree utilities used in compiler-like data structures.

## Overview

This repository contains two integrated C++ components:

- `expression_dag` — a DAG-backed expression registry that assigns stable IDs to expressions, tracks operand dependencies, reuses equivalent pure expressions, and exposes immutable snapshots.
- `avl_tree` — an AVL tree implementation used to manage keyed expression metadata with balancing and validation checks.

- `identifier_trie` — a Qt-free identifier index (case-sensitive) with exact lookup, prefix search and edit-distance spelling suggestions; it stores spellings and their `SymbolId`s only.

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

## Integration status

The project is fully wired together through CMake:

- `expression_dag` builds as a library target.
- `avl_tree` builds as a separate library target.
- Both test executables are linked to their respective libraries and added to CTest.
- The repository currently builds cleanly and passes all tests in the default configuration.

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
