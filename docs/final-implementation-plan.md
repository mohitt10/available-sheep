# Final Implementation Plan — Compiler Optimization Engine

**Reference architecture:** [High-level design](./Screenshot%20From%202026-10-21%2015-20-08.png)  
**Target:** C++17/20 and Qt 6  
**Execution model:** All compilation and optimization processing runs locally.

This is the project-level implementation plan. It follows the four logical
layers and named components in the HLD. The three data structures already in
the repository are Layer 4 components; they are not the whole application.

## 1. Project objective and scope

Build a desktop compiler optimization application that lets a user enter a
restricted C-like program, see its initial intermediate representation (IR),
run a configurable optimization pipeline, and inspect the optimized IR,
optimization results, and the data structures used by the engine.

The delivered application comprises all four HLD layers:

1. **Presentation (Qt 6):** optimization controls, source editor, IR viewer,
   data-structure visualizer, and optimization results.
2. **Compiler frontend:** lexer, parser, AST, and IR generator.
3. **Optimization engine:** constant folding, constant propagation, common
   subexpression elimination (CSE), and dead-code elimination, iterated until
   convergence or the configured limit.
4. **Data-structure engine:** identifier trie, expression DAG, and AVL index.

The user-facing source language is deliberately restricted to the subset needed
by the project. Before frontend implementation, define that subset explicitly:
supported types, literals, identifiers, operators, assignments, control-flow
constructs (if any), and diagnostics. Do not imply support for arbitrary C.

## 2. HLD architecture and data flow

### Layer 1 — Presentation (Qt 6)

Implement the five HLD presentation areas:

- **Optimization controls:** run action, pass options, and iteration limit.
- **Source code editor:** accepts the documented restricted C-like input.
- **IR viewer:** displays initial and optimized IR.
- **Data-structure visualizer:** displays DAG, AVL, and trie views.
- **Optimization results:** displays before/after statistics.

The UI passes source and run options into the local engine. It receives
read-only result values and snapshots for display; it must not mutate internal
IR or data-structure nodes. Keep Qt types out of compiler/data-structure public
APIs.

### Layer 2 — Compiler frontend

The source-to-IR pipeline is:

`source code → lexer (tokens) → parser (syntax) → AST (syntax tree) → IR generator → initial IR artifact`

The lexer also sends recognized identifiers to the trie. The frontend owns
lexical and syntactic diagnostics and must not start optimization when source
cannot be translated to a valid initial IR.

### Layer 3 — Optimization engine

The initial IR enters the ordered pipeline shown in the HLD:

1. **Constant folding:** evaluate expressions whose operands are compile-time
   constants.
2. **Constant propagation:** substitute known values for identifiers where
   valid, using trie-backed identifier lookup as shown in the HLD.
3. **Common subexpression elimination:** find reusable equivalent expressions,
   using the DAG to represent expression nodes/dependencies and the AVL tree
   for ordered expression metadata and lookup.
4. **Dead-code elimination:** remove unused results while preserving required
   side effects.

Passes run in this order and repeat until no pass changes the IR or the
user-specified iteration limit is reached. Produce an optimized IR artifact in
either case, and report whether the run converged or stopped at the limit.
The engine provides before/after statistics to the presentation layer.

### Layer 4 — Data-structure engine

- **Trie:** identifier lookup, prefix search, and spelling suggestions; fed by
  frontend identifiers and queried by optimizer identifier lookup.
- **DAG:** expression nodes and dependencies; supports common-subexpression
  discovery and CSE rewrites.
- **AVL tree:** ordered expression metadata and efficient expression lookup;
  records rotations for the visualizer.

The optimizer queries and updates these structures through value-oriented
interfaces. The GUI consumes read-only result snapshots. The DAG owns
expression structure; the AVL index must not duplicate the DAG or become the
source of truth for expression dependencies.

## 3. Contracts to settle before implementation

Record these contracts in public headers and project documentation before
connecting the layers:

- **Language subset:** grammar, types, operator precedence, supported
  statements, and error behavior for unsupported syntax.
- **IR:** instruction/value model, operand representation, type representation,
  control-flow representation if needed, side-effect classification, and
  serialization/pretty-print format.
- **IDs:** one canonical shared definition for `SymbolId`, `ExprId`, and
  `TypeId`; use `std::uint32_t` for symbol/type IDs and `std::uint64_t` for
  expression IDs unless the IR contract requires otherwise.
- **Purity and equivalence:** which operations can be shared, which are
  effectful, and every semantic field included in an expression key. Operand
  order is significant unless explicitly normalized for a proven-commutative
  operation.
- **Pass behavior:** legality conditions, side-effect rules, pass enablement,
  ordering, convergence detection, iteration-limit behavior, and statistics.
- **UI result model:** immutable initial/optimized IR, diagnostics, pass
  summaries, before/after metrics, and value snapshots for each visualizer.

## 4. Phased implementation

### Phase 1 — Contracts, build, and project skeleton

1. Confirm the supported C++ standard (C++17 baseline, with C++20 only if
   required), Qt 6 version, native compilers, and supported operating systems.
2. Define the language subset, IR, shared IDs, pass semantics, and UI result
   model above.
3. Organize the project into clear frontend, optimizer, data-structure, and
   presentation modules while preserving the existing component APIs where
   they meet the agreed contracts.
4. Configure CMake for a native desktop compiler by default. Cross-compilation
   must be explicit; do not let an embedded toolchain be selected accidentally.
5. Keep component and application tests runnable through CTest.

**Exit criteria:** The application scope and contracts are written down; a
native CMake configure/build works; Qt 6 and test discovery are verified.

### Phase 2 — Layer 4: complete the data structures

#### Shared types

- Move `ExprId`, `TypeId`, and `SymbolId` into the canonical shared types
  header; remove duplicate aliases.
- Keep all public data-structure APIs Qt-free and return copied value types for
  snapshots/events.

#### Trie

- Preserve exact lookup, prefix search, deterministic bounded spelling
  suggestions, multiple symbol IDs per spelling, and case-sensitive matching.
- Define how the frontend registers identifiers and how symbol IDs map to
  declarations/scopes. Scope resolution belongs to the frontend, not the trie.

#### DAG

- Preserve stable expression IDs, operand dependencies, pure-expression reuse,
  and non-merging of effectful expressions.
- Include type and all relevant semantic attributes in structural equivalence.
- Expose deterministic immutable expression snapshots for optimizer
  diagnostics and GUI visualization.

#### AVL expression index

- Align the AVL API with the HLD: index expression metadata by shared `ExprId`,
  support lookup/update/deletion, expose ordered value snapshots, and record
  typed rotation events for visualization.
- Validate global ordering, stored heights, AVL balance, and node count.
- Do not use string keys or duplicate DAG dependency ownership in the expression
  index.

**Exit criteria:** Each structure has a stable Qt-free API, focused tests, and
the data needed by the optimizer and visualizer. Trie symbols and expression
IDs remain distinct types.

### Phase 3 — Layer 2: frontend and initial IR

1. Implement a lexer for the documented source subset. Produce typed tokens,
   source locations, and useful lexical diagnostics; feed identifier spellings
   and `SymbolId`s into the trie.
2. Implement a parser that consumes tokens and reports syntax errors without
   crashing or producing a misleading successful result.
3. Build an AST that represents the supported language constructs and source
   locations needed for diagnostics.
4. Implement AST-to-IR generation. Define deterministic IR IDs, types,
   operands, and effect/side-effect information according to the agreed IR
   contract.
5. Emit and retain the initial IR artifact before optimization.

**Exit criteria:** Valid sample programs produce deterministic initial IR;
invalid input produces actionable diagnostics; lexer/parser/AST/IR tests cover
the supported grammar and failure cases.

### Phase 4 — Layer 3: optimization engine

1. Define a pass interface that consumes and updates IR while reporting whether
   it changed the program and any useful pass statistics.
2. Implement **constant folding** for supported operations, respecting types,
   overflow/division behavior, and invalid constant operations as specified by
   the language contract.
3. Implement **constant propagation** with correct binding/scope semantics,
   querying identifier resolution through the frontend/trie contract.
4. Implement **CSE** by constructing/querying the DAG, reusing only equivalent
   pure expressions, and applying replacements to IR uses. Keep effectful
   operations distinct.
5. Implement **dead-code elimination** using IR use information, preserving
   observable side effects and required program outputs.
6. Run enabled passes in the HLD order and repeat until a fixed point or the
   configured iteration limit. Handle zero limits, no-change iterations, and
   repeated state detection consistently with the chosen contract.
7. Build/update AVL metadata for expression lookup and record rotation events
   independently of optimization correctness.
8. Produce optimized IR plus convergence status, per-pass results, and
   before/after statistics.

**Exit criteria:** Each pass has focused semantic tests; the pipeline tests
demonstrate multi-pass convergence, iteration-limit stopping, and preservation
of observable behavior for the supported language subset.

### Phase 5 — Layer 1: Qt presentation

1. Build the source editor and optimization controls (run, pass options,
   iteration limit).
2. Connect the run action to the local frontend/optimizer workflow without
   blocking the UI during nontrivial runs.
3. Display diagnostics and initial versus optimized IR.
4. Visualize DAG nodes/dependencies, AVL metadata/rotation events, and trie
   lookup/prefix/suggestion state using read-only snapshots.
5. Display before/after optimization statistics and whether optimization
   converged or reached its limit.
6. Keep UI formatting and Qt ownership in the presentation layer; do not leak
   widget or Qt container types into core APIs.

**Exit criteria:** A user can enter a supported program, configure and run the
pipeline, inspect diagnostics and both IR artifacts, view all three structures,
and read the result statistics.

### Phase 6 — End-to-end integration and release readiness

1. Add end-to-end tests from source text through initial IR, optimization
   passes, optimized IR, and reported results.
2. Verify identifier insertion/lookup, DAG CSE queries, and AVL metadata
   updates use the agreed IDs and contracts.
3. Verify presentation snapshots match engine results and cannot mutate
   internal state.
4. Test pass combinations, malformed source, effectful expressions,
   optimization limits, and representative before/after examples.
5. Run build and tests in Debug and Release with the documented native toolchain;
   confirm CTest discovers all component, pass, integration, and UI tests.
6. Document build/run instructions, supported language subset, pass semantics,
   limitations, and known platform requirements.

**Exit criteria:** The HLD's user-visible workflow works end to end, automated
tests pass in supported configurations, and documentation reflects verified
behavior.

## 5. Acceptance checklist mapped to the HLD

- [ ] **Layer 1:** Qt 6 provides optimization controls, restricted C-like source
  editor, initial/optimized IR viewer, DAG/AVL/trie visualizer, and before/after
  results.
- [ ] **Layer 2:** Lexer, parser, AST, and IR generator produce an initial IR
  artifact and useful diagnostics; identifiers are fed to the trie.
- [ ] **Layer 3:** Constant folding, constant propagation, CSE, and dead-code
  elimination run in the specified order until convergence or the configured
  limit; optimized IR and statistics are produced.
- [ ] **Layer 4:** Trie, DAG, and AVL support their stated responsibilities,
  share canonical types where applicable, and expose no mutable internals to UI.
- [ ] Optimization queries/updates flow through the data structures; GUI reads
  snapshots only.
- [ ] All processing is local; no remote service is required for compilation or
  optimization.
- [ ] Native build, component tests, optimizer tests, end-to-end tests, and
  supported UI tests pass in documented configurations.

## 6. Current implementation baseline and recommended order

The repository currently contains the three Layer 4 libraries and focused
tests, but not the HLD's Qt presentation, compiler frontend, or optimization
pipeline. The trie and DAG substantially implement their standalone
responsibilities. The AVL implementation is currently string-keyed and does
not yet satisfy the HLD's `ExprId`-keyed expression-metadata role. The current
CMake configuration registers three component tests; a previous build attempt
selected an ARM bare-metal compiler and failed during compiler validation, so
native build/test success is not yet verified.

Recommended order: Phase 1 contracts/build; Phase 2 shared types and completed
data structures; Phase 3 frontend and initial IR; Phase 4 optimization passes
and fixed-point driver; Phase 5 Qt presentation; Phase 6 end-to-end validation.
This sequence follows the HLD's dependencies: the optimizer consumes frontend
IR and the data structures, while the UI displays results from all preceding
layers.

## 7. Three-person implementation breakdown

This breakdown assigns the remaining work to three people while all changes
are integrated on one shared branch. It supplements the phase order above; it
does not change the technical contracts or exit criteria.

### Person 1 — Language frontend and initial IR

**Owns:** language-subset documentation, lexer, parser, AST, frontend
diagnostics, semantic validation, and initial IR generation.

**Phase 1 — Contracts and skeleton**

- Draft the supported language grammar, type/literal rules, operator
  precedence, supported statements, and unsupported-syntax behavior.
- Specify source locations and the frontend diagnostic value type.
- Agree with Person 2 on canonical `SymbolId`/`TypeId` definitions and with
  Person 3 on how diagnostics and initial IR are represented in UI results.
- Add the frontend module and test target once the shared directory and build
  layout are agreed.

**Phase 2 — Frontend and initial IR**

- Implement typed tokens and a lexer with locations and lexical diagnostics.
- Implement AST types and a parser for only the documented grammar.
- Add semantic validation for declarations, identifier use, and type rules
  supported by the language.
- Register identifier spellings and symbol IDs in the trie using its public
  API; keep declaration and scope resolution in the frontend.
- Generate deterministic initial IR with typed operands and explicit effect
  information.
- Retain the initial IR separately from the optimizer's output.
- Add focused lexer, parser, semantic-validation, and IR-generation tests,
  including malformed and unsupported input.

**Phase 3 — Integration and validation**

- Stabilize the frontend API and sample programs for Person 2's optimizer and
  Person 3's UI.
- Verify invalid input returns diagnostics and does not start optimization.
- Add or support source-to-initial-IR integration tests.
- Update the language documentation and examples to match implemented behavior.

**Handoff/acceptance:** A documented supported program parses into deterministic
initial IR; invalid programs produce source-located diagnostics; the optimizer
can consume the IR without depending on frontend implementation details.

### Person 2 — Data structures and optimization engine

**Owns:** canonical shared IDs, Layer 4 completion, optimizer IR transformations,
pass orchestration, convergence reporting, and optimization statistics.

**Phase 1 — Contracts and skeleton**

- Own the shared ID header and agree its definitions with Persons 1 and 3
  before either frontend or UI consumes them.
- Specify expression purity/equivalence fields, pass result/statistics types,
  and fixed-point/iteration-limit behavior with the team.
- Keep data-structure and optimizer public APIs Qt-free.
- Add optimizer/data-structure targets and tests to the agreed native CMake
  layout.

**Phase 2 — Data structures**

- Audit the trie, DAG, and AVL against the Layer 4 requirements and preserve
  already-correct behavior.
- Align the AVL expression index to shared `ExprId` keys, with metadata
  lookup/update/deletion, ordered snapshots, and typed rotation events.
- Ensure the DAG owns expression dependencies, merges only equivalent pure
  expressions, and exposes deterministic copied snapshots.
- Ensure the trie supports the frontend's spelling-to-symbol registration and
  lookup contract without taking on scope resolution.
- Add focused tests for ID distinctions, ordering, balance/heights, rotations,
  equivalence, effectful expressions, and snapshot values.

**Phase 3 — Optimization engine**

- Define the pass interface and implement constant folding, constant
  propagation, CSE, and dead-code elimination in the specified order.
- Use the DAG for expression dependencies/reuse and the AVL for ordered
  expression metadata; use trie lookup only through the agreed identifier
  contract.
- Preserve observable side effects and required outputs; implement the
  language-defined type, overflow, and invalid-operation rules.
- Add a fixed-point driver with pass enablement, iteration-limit handling,
  convergence status, and before/after and per-pass statistics.
- Test each pass independently, then test multi-pass convergence, no-change
  runs, zero and reached limits, and preservation of observable behavior.

**Phase 4 — Integration and validation**

- Integrate against Person 1's initial IR and report interface mismatches
  before changing shared contracts.
- Provide immutable optimized IR and visualizer snapshots to Person 3.
- Add source-to-optimized-IR tests and verify repeatable output/status/statistics.

**Handoff/acceptance:** All four passes operate on the frontend IR, use the
data structures according to their responsibilities, and return optimized IR,
per-pass results, convergence status, and statistics with semantic tests.

### Person 3 — Qt presentation and application integration

**Owns:** Qt application structure, controls/editor, result presentation,
snapshot-based visualizers, end-to-end wiring, and user-facing build/run
documentation.

**Phase 1 — Contracts and application shell**

- Confirm the supported Qt 6 version and native desktop toolchain with the
  team; help verify Qt discovery and CTest integration in CMake.
- Define the UI-facing result model with Persons 1 and 2: diagnostics, initial
  and optimized IR, pass summaries, metrics, convergence status, and copied
  trie/DAG/AVL snapshots.
- Set up the Qt application target and a minimal shell that builds without
  leaking Qt types into frontend, optimizer, or data-structure APIs.
- Agree on the run boundary: the UI submits source/options and receives result
  values; it never edits engine IR or nodes.

**Phase 2 — Presentation**

- Implement the source editor, run action, pass options, and iteration-limit
  control.
- Display source-located diagnostics and separate initial and optimized IR.
- Display convergence/limit status, per-pass summaries, and before/after
  metrics.
- Render DAG dependencies, AVL ordered metadata/rotation events, and trie
  lookup/prefix/suggestion state from copied snapshots only.
- Add UI tests for result rendering and diagnostics where supported by the
  project’s Qt test setup.

**Phase 3 — End-to-end integration**

- Connect the run action to the frontend and optimizer APIs after their
  contracts are stable.
- Keep the UI responsive during nontrivial runs, using the project’s chosen
  Qt execution pattern and safe result handoff.
- Verify frontend failures display diagnostics without presenting stale or
  success-shaped optimization results.
- Add or coordinate end-to-end UI workflow tests and verify displayed values
  match engine results.

**Phase 4 — Release readiness**

- Verify a clean native Debug and Release configure/build and CTest discovery
  with the team; do not treat the existing build directory as proof of a
  native configuration.
- Document verified build/run steps, platform requirements, supported
  language, pass behavior, and known limitations.
- Coordinate the final acceptance checklist and resolve UI/integration defects
  with the responsible owner.

**Handoff/acceptance:** A user can enter supported source, run the local
pipeline, and inspect diagnostics, both IR artifacts, optimization results,
and all three read-only visualizations.

### Shared phase gates and one-branch coordination

1. **Contract gate:** All three agree on language, IR, shared IDs, pass/result
   models, and module boundaries before dependent APIs are implemented.
2. **Data-structure gate:** Person 2 publishes stable Qt-free APIs and tests;
   Person 1 uses the trie contract while building the frontend.
3. **Frontend/optimizer gate:** Person 1 publishes deterministic initial IR;
   Person 2 validates the optimizer against it; Person 3 can finish UI layout
   against the agreed result model.
4. **Integration gate:** Person 3 wires only the stable public APIs; the three
   owners jointly resolve cross-layer changes and add end-to-end coverage.
5. **Release gate:** All owners verify the acceptance checklist, native build,
   tests, and documentation against the working application.

All three people commit directly to the same shared branch. To avoid
conflicting edits, assign one owner per file/module, agree shared-header or
CMake changes before editing them, and integrate small changes frequently.
Do not duplicate or independently redefine shared types and result contracts.
When an interface change is necessary, its owner proposes the change, the
dependent owners update their call sites in the same integration step, and
the relevant tests are run before continuing.
