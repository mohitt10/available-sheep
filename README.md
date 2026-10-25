# COMPILER OPTIMIZATION ENGINE

A small C++ project for expression-graph and AVL-tree utilities used in compiler-like data structures.

## Overview

This repository contains several standalone C++ components:

- `expression_dag` — a DAG-backed expression registry that assigns stable IDs to expressions, tracks operand dependencies, reuses equivalent pure expressions, and exposes immutable snapshots.
- `avl_tree` — an AVL tree implementation used to manage keyed expression metadata with balancing and validation checks.

- `identifier_trie` — a Qt-free identifier index (case-sensitive) with exact lookup, prefix search and edit-distance spelling suggestions; it stores spellings and their `SymbolId`s only.
- `ir` — an intermediate-representation model with instruction validation, formatting, arithmetic evaluation, metrics, and an interpreter for programs with input/output.
- `frontend` — a small lexer, parser, semantic analyzer, and IR generator for integer declarations, assignments, arithmetic/comparison expressions, input, and print statements.

The project is structured as a single CMake build with library targets, a Qt 6
desktop application, and automated tests.

## Repository layout

- `include/expression_dag.hpp` — expression DAG public API and types.
- `src/expression_dag.cpp` — DAG node storage and graph logic.
- `include/Avltree.hpp` — AVL tree interface.
- `src/Avltree.cpp` — AVL balancing, insertion, deletion, and validation logic.
- `tests/expression_dag_test.cpp` — DAG behavior and correctness checks.
- `include/identifier_trie.hpp` / `src/identifier_trie.cpp` — identifier trie API and implementation.
- `include/shared_types.hpp` — shared `SymbolId` definition (the single alias for symbol IDs).
- `tests/identifier_trie_test.cpp` — trie behavior checks (including a brute-force comparison for suggestions).
- `include/Ir.hpp` / `src/Ir.cpp` — IR instructions, validation, evaluation, formatting, and metrics.
- `include/Interpreter.hpp` / `src/Interpreter.cpp` — IR program interpreter.
- `include/Token.hpp`, `include/Ast.hpp`, and the frontend headers/sources — tokenization, parsing, semantic checks, and IR generation.
- `include/Diagnostics.hpp`, `include/Optimizertypes.hpp`, and `include/Resultmodel.hpp` — diagnostics and optimization-result data types.
- `tests/Irtests.cpp` — IR and interpreter behavior checks.
- `tests/Frontendtest.cpp` — end-to-end source compilation and frontend checks.
- `tests/avl_test.cpp` — AVL tree insertion, balancing, and validation checks.
- `CMakeLists.txt` — workspace build configuration and CTest registration.
- `app/` — Qt 6 Widgets application for source entry, frontend diagnostics, and initial IR display.

## Integration status

The project is fully wired together through CMake:

- `expression_dag`, `avl_tree`, `identifier_trie`, `ir`, and `frontend` build as separate library targets; `frontend` links to `ir`.
- `available_sheep_app` provides source editing, frontend compilation, diagnostics, and initial IR display.
- Each component has a corresponding test executable registered with CTest.
- The optimizer header currently provides pass configuration and result data types; optimization passes are not implemented yet.

The application reflects this status: optimization controls are disabled,
optimized IR is not represented as if it had been produced, and the DAG/AVL/trie
tabs report that engine snapshots are not yet available.

## Build and test

From the project root:

```sh
cmake --preset host
cmake --build --preset host
ctest --preset host
```

The `host` preset is for Linux with `/usr/bin/g++`, Ninja, and Qt 6 Widgets
installed. CMake 3.21 or newer is required to use presets.

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

The `ir` library exposes `Program` and `Instruction` types, validation and formatting helpers, arithmetic evaluation, metrics, and `run()` for interpreting a program with supplied input values.

## Notes

- `ExpressionDag` treats purity as caller-defined; equivalent pure expressions are merged only when the caller chooses `addPureExpression`.
- Operand IDs must already exist in the graph, so the DAG stays acyclic and deterministic.
- The AVL tree validates structural balance and maintains a rotation log for debugging.
