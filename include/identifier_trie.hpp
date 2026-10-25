// IdentifierTrie - Layer 4 identifier index (exact lookup, prefix search,
// spelling suggestions).
//
// Boundaries
//   * Owns spellings and their SymbolIds only. It does not parse source, decide
//     lexical scope, or own symbol declarations.
//   * The public API is Qt-free and returns copies; no caller can reach a node.
//
// Semantics
//   * Matching is case-sensitive and byte-based (ASCII identifiers are the
//     expected case; other bytes, e.g. UTF-8, are stored and compared verbatim).
//   * Empty spellings are not valid identifiers: insert("") is a no-op, and
//     contains("") / findExact("") report "not found". An empty *prefix* is
//     valid and matches every spelling; an empty suggest() input is valid and
//     is at edit distance == length from each spelling.
//   * One spelling may carry several SymbolIds (shadowing, different scopes).
//     findExact returns them in insertion order; re-inserting an identical
//     (spelling, id) pair is idempotent. Picking the active binding is the
//     frontend's job.
//   * Multi-result queries are deterministic and bounded by `limit`
//     (limit == 0 yields an empty result).
//
// Child representation (design tradeoff)
//   Each node keeps its outgoing edges in a small vector sorted by byte.
//   - A flat per-node array (64 or 256 slots) gives O(1) steps but wastes
//     memory: most nodes of an identifier trie have only 1-3 children.
//   - std::map / unordered_map per node cost an allocation per edge and have
//     poor locality; unordered_map also iterates in unspecified order.
//   - A sorted vector gives O(log k) lookup for k children (k is tiny), compact
//     storage, and free lexicographic iteration, which makes prefix results and
//     suggestion tie-breaking deterministic without any extra sorting.
//   The node layout lives entirely in the .cpp, so it can change without
//   touching callers.
//
// Thread-safety: concurrent const calls are safe; insert() needs external
// synchronisation.
#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "shared_types.hpp"

namespace available_sheep {

namespace detail {
struct TrieNode;  // defined in identifier_trie.cpp; never exposed
}

// Plain value type for GUI / diagnostics snapshots.
struct TrieEntry {
    std::string spelling;
    std::vector<SymbolId> symbols;
};

class IdentifierTrie {
public:
    // Default maximum Levenshtein distance accepted by suggest().
    static constexpr std::size_t kDefaultMaxEditDistance = 2;

    IdentifierTrie();
    ~IdentifierTrie();
    IdentifierTrie(IdentifierTrie&&) noexcept;
    IdentifierTrie& operator=(IdentifierTrie&&) noexcept;
    IdentifierTrie(const IdentifierTrie&) = delete;
    IdentifierTrie& operator=(const IdentifierTrie&) = delete;

    // Associates `symbol` with `spelling`.
    // Empty spelling and duplicate (spelling, symbol) pairs are ignored.
    void insert(std::string_view spelling, SymbolId symbol);

    bool contains(std::string_view spelling) const;

    // All SymbolIds indexed for `spelling`, in insertion order. Empty if unknown.
    std::vector<SymbolId> findExact(std::string_view spelling) const;

    // Up to `limit` indexed spellings starting with `prefix`, in lexicographic
    // (unsigned byte) order. The prefix itself is included if it is indexed.
    std::vector<std::string> findByPrefix(std::string_view prefix,
                                          std::size_t limit) const;

    // Up to `limit` indexed spellings within Levenshtein distance
    // `maxEditDistance` of `spelling`, ordered by (distance, lexicographic).
    // An exact match (distance 0) is included if present; callers normally call
    // this only after an exact lookup failed.
    std::vector<std::string> suggest(
        std::string_view spelling, std::size_t limit,
        std::size_t maxEditDistance = kDefaultMaxEditDistance) const;

    // Number of distinct indexed spellings.
    std::size_t size() const noexcept { return spellingCount_; }
    bool empty() const noexcept { return spellingCount_ == 0; }

    // Copied, lexicographically ordered view of the whole index for the GUI.
    std::vector<TrieEntry> snapshot() const;

private:
    std::unique_ptr<detail::TrieNode> root_;
    std::size_t spellingCount_ = 0;
};

}  // namespace available_sheep
