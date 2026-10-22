# Implementation Plan: Identifier Trie

**Owner:** Person 1  
**HLD component:** Layer 4 — Trie  
**Reference:** [HLD image](<./Screenshot From 2026-10-21 15-20-08.png>)

**Target:** C++17/20. Keep Qt6 in the presentation/visualization layer, not in
the trie API.

## Goal and boundaries

Implement the identifier index used by the compiler frontend and optimizer for:

- Exact identifier lookup.
- Prefix search.
- Spelling suggestions for unknown identifiers.

The trie owns identifier spellings and their associated `SymbolId` values. It does
not parse source, decide lexical scope, or own symbol declarations. The frontend
continues to decide which symbol a spelling refers to in the current scope.

## Proposed integration contract

Agree on the shared ID definition before implementation:

```cpp
using SymbolId = std::uint32_t;
```

Keep the public interface independent of Qt so it can be used by the frontend
and optimizer. Suggested API:

```cpp
class IdentifierTrie {
public:
    void insert(std::string_view spelling, SymbolId symbol);
    bool contains(std::string_view spelling) const;
    std::vector<SymbolId> findExact(std::string_view spelling) const;
    std::vector<std::string> findByPrefix(
        std::string_view prefix, std::size_t limit) const;
    std::vector<std::string> suggest(
        std::string_view spelling, std::size_t limit) const;
};
```

`findExact` returns all indexed symbols for a spelling; the frontend applies
scope-resolution rules to choose the active binding. Identifier matching is
case-sensitive, consistent with C-like source. Suggestions use edit distance,
ordered by distance and then lexicographically; prefix and suggestion results
are deterministic and bounded by `limit`.

## Implementation steps

1. Agree with the other owners on `SymbolId` and place it in the shared compiler
   types header; do not define competing ID aliases in this component.
2. Implement a trie with character edges, terminal symbol IDs, and safe handling
   of empty input. Choose a child representation that fits the expected alphabet
   and document the tradeoff.
3. Implement exact lookup and prefix enumeration. Avoid returning references to
   internal storage from the public API.
4. Implement spelling suggestions using edit distance; prune work once enough
   suitable suggestions are known, and order results by distance then
   lexicographically.
5. Add unit tests, including duplicate spellings with distinct IDs, missing
   identifiers, shared prefixes, case sensitivity, empty input, and result
   limits.
6. Expose read-only query methods to frontend/optimizer callers. If the GUI needs
   a snapshot, provide copied values rather than exposing trie nodes.

## Ownership and handoffs

- Own the trie implementation, its focused tests, and its component-level API.
- Frontend integration: insert identifiers as they are encountered and apply
  scope-resolution rules to the IDs returned by `findExact`.
- Optimizer integration: use the existing symbol-resolution contract for
  identifier lookup; do not infer or mutate IR dependencies through the trie.
- GUI integration: provide copied prefix/suggestion results; the Qt visualizer
  must not depend on the internal node layout.
- Coordinate the shared `SymbolId` definition and header path before parallel
  implementation starts.

## Definition of done

- The public API is usable without Qt and follows the shared ID definition.
- Exact lookup, prefix search, and suggestions have deterministic behavior and
  pass focused unit tests.
- No caller needs access to internal trie nodes.
- Agree on source/header paths and register the focused tests with the project's
  build system once the source scaffold is created.
