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
- `include/Optimizer.hpp` / `src/Optimizer.cpp` — optimization passes, fixed-point driver, DAG and AVL snapshots.
- `include/Token.hpp`, `include/Ast.hpp`, and the frontend headers/sources — tokenization, parsing, semantic checks, and IR generation.
- `include/Diagnostics.hpp`, `include/Optimizertypes.hpp`, and `include/Resultmodel.hpp` — diagnostics and optimization-result data types.
- `tests/Irtests.cpp` — IR and interpreter behavior checks.
- `tests/Frontendtest.cpp` — end-to-end source compilation and frontend checks.
- `tests/avl_test.cpp` — AVL tree insertion, balancing, and validation checks.
- `CMakeLists.txt` — workspace build configuration and CTest registration.
- `app/` — Qt 6 Widgets application for source entry, diagnostics, initial/optimized IR, and engine snapshots.

## Supported language and integration

The frontend accepts integer declarations with initializers, assignments, and
`print(expr);` statements. Expressions support `input()`, parentheses, unary
`-`/`!`, arithmetic, and integer comparisons. Line (`//`) and block (`/* */`)
comments are accepted. Integers use signed 32-bit wrapping arithmetic; division
and remainder by zero remain runtime traps.

The frontend emits deterministic, typed Int32 IR with source locations and
effect classifications. The optimizer runs constant folding, constant
propagation, common-subexpression elimination, and dead-code elimination in
that order until a no-change iteration, a repeated state, or the configured
iteration limit. Each result retains both the initial and optimized IR, pass
statistics, metrics, and copied DAG/AVL/trie snapshots.

`expression_dag`, `avl_tree`, `identifier_trie`, `ir`, `frontend`, and
`optimizer` build as separate libraries. When Qt 6 Widgets is installed, CMake
also builds `available_sheep_app`, which exposes the pass controls and presents
diagnostics, IR, convergence details, and data-structure snapshots.

## Build and test

From the project root:

```sh
cmake --preset host
cmake --build --preset host
ctest --preset host
```

The `host` preset is for Linux with `/usr/bin/g++` and Ninja. Qt 6 Widgets is
optional for the libraries and tests; without it, CMake skips the desktop app.
CMake 3.21 or newer is required to use presets.

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
