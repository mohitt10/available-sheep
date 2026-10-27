#include "identifier_trie.hpp"

#include <algorithm>
#include <utility>

namespace available_sheep {
namespace detail {

// A node's outgoing edges are sorted ascending by byte label. `symbols` is
// non-empty exactly when a spelling terminates at this node.
struct TrieNode {
    struct Edge {
        unsigned char label;
        std::unique_ptr<TrieNode> child;
    };

    std::vector<Edge> edges;
    std::vector<SymbolId> symbols;

    bool terminal() const noexcept { return !symbols.empty(); }

    const TrieNode* find(unsigned char c) const {
        const auto it = lowerBound(c);
        return (it != edges.end() && it->label == c) ? it->child.get() : nullptr;
    }

    TrieNode& findOrCreate(unsigned char c) {
        auto it = std::lower_bound(edges.begin(), edges.end(), c, labelLess);
        if (it == edges.end() || it->label != c) {
            it = edges.insert(it, Edge{c, std::make_unique<TrieNode>()});
        }
        return *it->child;
    }

private:
    static bool labelLess(const Edge& e, unsigned char label) {
        return e.label < label;
    }
    std::vector<Edge>::const_iterator lowerBound(unsigned char c) const {
        return std::lower_bound(edges.begin(), edges.end(), c, labelLess);
    }
};

}  // namespace detail

namespace {

using detail::TrieNode;

const TrieNode* walk(const TrieNode* node, std::string_view key) {
    for (const unsigned char c : key) {
        node = node->find(c);
        if (node == nullptr) return nullptr;
    }
    return node;
}

// Pre-order DFS over sorted edges == lexicographic order of spellings
// (a spelling precedes all of its extensions).
void collectSpellings(const TrieNode& node, std::string& buf, std::size_t limit,
                      std::vector<std::string>& out) {
    if (out.size() >= limit) return;
    if (node.terminal()) out.push_back(buf);
    for (const auto& edge : node.edges) {
        if (out.size() >= limit) return;
        buf.push_back(static_cast<char>(edge.label));
        collectSpellings(*edge.child, buf, limit, out);
        buf.pop_back();
    }
}

void collectEntries(const TrieNode& node, std::string& buf,
                    std::vector<TrieEntry>& out) {
    if (node.terminal()) out.push_back(TrieEntry{buf, node.symbols});
    for (const auto& edge : node.edges) {
        buf.push_back(static_cast<char>(edge.label));
        collectEntries(*edge.child, buf, out);
        buf.pop_back();
    }
}

// ---------------------------------------------------------------------------
// Suggestions: Levenshtein distance computed incrementally down the trie.
//
// Each trie node gets one DP row (distance from the node's path to every prefix
// of the target), derived from its parent's row in O(|target|). Identifier
// recursion depth equals identifier length, which is small.
//
// Pruning
//   * rowMin lower-bounds the distance of every descendant, so a subtree is
//     skipped when even rowMin cannot be accepted.
//   * We keep the best `limit` candidates in a max-heap keyed by
//     (distance, spelling). Because DFS visits spellings in lexicographic
//     order, a later candidate with the *same* distance as the current worst
//     is lexicographically larger and can never displace it. So once the heap
//     is full, only strictly smaller distances are accepted/explored.
// ---------------------------------------------------------------------------
struct Candidate {
    std::size_t distance;
    std::string spelling;
    bool operator<(const Candidate& o) const {
        return distance != o.distance ? distance < o.distance
                                      : spelling < o.spelling;
    }
};

class SuggestSearch {
public:
    SuggestSearch(std::string_view target, std::size_t limit,
                  std::size_t maxDistance)
        : target_(target), limit_(limit), maxDistance_(maxDistance) {}

    std::vector<std::string> run(const TrieNode& root) {
        std::vector<std::size_t> row(target_.size() + 1);
        for (std::size_t j = 0; j < row.size(); ++j) row[j] = j;
        visit(root, row);

        std::sort_heap(heap_.begin(), heap_.end());  // ascending
        std::vector<std::string> out;
        out.reserve(heap_.size());
        for (auto& c : heap_) out.push_back(std::move(c.spelling));
        return out;
    }

private:
    bool canAccept(std::size_t distance) const {
        if (heap_.size() < limit_) return distance <= maxDistance_;
        return distance < heap_.front().distance;
    }

    void offer(std::size_t distance) {
        if (!canAccept(distance)) return;
        if (heap_.size() < limit_) {
            heap_.push_back({distance, buf_});
            std::push_heap(heap_.begin(), heap_.end());
        } else {
            std::pop_heap(heap_.begin(), heap_.end());
            heap_.back() = {distance, buf_};
            std::push_heap(heap_.begin(), heap_.end());
        }
    }

    void visit(const TrieNode& node, const std::vector<std::size_t>& row) {
        if (node.terminal()) offer(row.back());

        std::vector<std::size_t> next(row.size());
        for (const auto& edge : node.edges) {
            next[0] = row[0] + 1;
            std::size_t rowMin = next[0];
            for (std::size_t j = 1; j < row.size(); ++j) {
                const std::size_t substitute =
                    row[j - 1] +
                    (static_cast<unsigned char>(target_[j - 1]) == edge.label ? 0
                                                                              : 1);
                next[j] = std::min({row[j] + 1, next[j - 1] + 1, substitute});
                rowMin = std::min(rowMin, next[j]);
            }
            if (!canAccept(rowMin)) continue;  // prune whole subtree

            buf_.push_back(static_cast<char>(edge.label));
            visit(*edge.child, next);
            buf_.pop_back();
        }
    }

    std::string_view target_;
    std::size_t limit_;
    std::size_t maxDistance_;
    std::string buf_;
    std::vector<Candidate> heap_;
};

}  // namespace

// ---------------------------------------------------------------------------
// IdentifierTrie
// ---------------------------------------------------------------------------
IdentifierTrie::IdentifierTrie() : root_(std::make_unique<detail::TrieNode>()) {}
IdentifierTrie::~IdentifierTrie() = default;
IdentifierTrie::IdentifierTrie(IdentifierTrie&& other) noexcept
    : root_(std::move(other.root_)), spellingCount_(other.spellingCount_) {
    // Leave the moved-from object valid and empty.
    other.root_ = std::make_unique<detail::TrieNode>();
    other.spellingCount_ = 0;
}
IdentifierTrie& IdentifierTrie::operator=(IdentifierTrie&& other) noexcept {
    if (this != &other) {
        root_ = std::move(other.root_);
        spellingCount_ = other.spellingCount_;
        other.root_ = std::make_unique<detail::TrieNode>();
        other.spellingCount_ = 0;
    }
    return *this;
}

void IdentifierTrie::insert(std::string_view spelling, SymbolId symbol) {
    if (spelling.empty()) return;

    detail::TrieNode* node = root_.get();
    for (const unsigned char c : spelling) node = &node->findOrCreate(c);

    auto& ids = node->symbols;
    if (std::find(ids.begin(), ids.end(), symbol) != ids.end()) return;
    if (ids.empty()) ++spellingCount_;
    ids.push_back(symbol);
}

bool IdentifierTrie::contains(std::string_view spelling) const {
    if (spelling.empty()) return false;
    const detail::TrieNode* node = walk(root_.get(), spelling);
    return node != nullptr && node->terminal();
}

std::vector<SymbolId> IdentifierTrie::findExact(std::string_view spelling) const {
    if (spelling.empty()) return {};
    const detail::TrieNode* node = walk(root_.get(), spelling);
    return node != nullptr ? node->symbols : std::vector<SymbolId>{};
}

std::vector<std::string> IdentifierTrie::findByPrefix(std::string_view prefix,
                                                      std::size_t limit) const {
    std::vector<std::string> out;
    if (limit == 0) return out;
    const detail::TrieNode* node = walk(root_.get(), prefix);
    if (node == nullptr) return out;

    std::string buf(prefix);
    collectSpellings(*node, buf, limit, out);
    return out;
}

std::vector<std::string> IdentifierTrie::suggest(
    std::string_view spelling, std::size_t limit,
    std::size_t maxEditDistance) const {
    if (limit == 0) return {};
    return SuggestSearch(spelling, limit, maxEditDistance).run(*root_);
}

std::vector<TrieEntry> IdentifierTrie::snapshot() const {
    std::vector<TrieEntry> out;
    out.reserve(spellingCount_);
    std::string buf;
    collectEntries(*root_, buf, out);
    return out;
}

}  // namespace available_sheep
