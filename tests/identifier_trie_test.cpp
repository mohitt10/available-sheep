// Focused tests for available_sheep::IdentifierTrie.
// Self-contained (no test framework) so it builds anywhere; each TEST body maps
// 1:1 onto a gtest/Catch2/QtTest case if the project standardises on one.

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "identifier_trie.hpp"

using available_sheep::IdentifierTrie;
using available_sheep::SymbolId;
using Strings = std::vector<std::string>;
using Ids = std::vector<SymbolId>;

namespace {

int g_failures = 0;
std::vector<std::pair<std::string, std::function<void()>>>& registry() {
    static std::vector<std::pair<std::string, std::function<void()>>> r;
    return r;
}
struct Registrar {
    Registrar(const char* name, std::function<void()> fn) {
        registry().emplace_back(name, std::move(fn));
    }
};

#define TEST(name)                                   \
    static void name();                              \
    static Registrar registrar_##name(#name, name);  \
    static void name()

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            ++g_failures;                                                    \
            std::cerr << "  CHECK failed: " #cond " (" << __FILE__ << ":"    \
                      << __LINE__ << ")\n";                                  \
        }                                                                    \
    } while (0)

template <class T>
bool eq(const std::vector<T>& a, const std::vector<T>& b) {
    return a == b;
}

// Reference Levenshtein for the differential test.
std::size_t levenshtein(const std::string& a, const std::string& b) {
    std::vector<std::size_t> prev(b.size() + 1), cur(b.size() + 1);
    for (std::size_t j = 0; j <= b.size(); ++j) prev[j] = j;
    for (std::size_t i = 1; i <= a.size(); ++i) {
        cur[0] = i;
        for (std::size_t j = 1; j <= b.size(); ++j) {
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1,
                               prev[j - 1] + (a[i - 1] == b[j - 1] ? 0u : 1u)});
        }
        std::swap(prev, cur);
    }
    return prev[b.size()];
}

}  // namespace

// --------------------------- exact lookup ----------------------------------

TEST(insert_and_exact_lookup) {
    IdentifierTrie t;
    t.insert("count", 1);
    t.insert("total", 2);
    CHECK(t.contains("count"));
    CHECK(t.contains("total"));
    CHECK(eq(t.findExact("count"), Ids{1}));
    CHECK(eq(t.findExact("total"), Ids{2}));
    CHECK(t.size() == 2);
    CHECK(!t.empty());
}

TEST(duplicate_spelling_distinct_ids_kept_in_insertion_order) {
    IdentifierTrie t;
    t.insert("x", 7);
    t.insert("x", 3);
    t.insert("x", 9);
    CHECK(eq(t.findExact("x"), Ids{7, 3, 9}));
    CHECK(t.size() == 1);  // one spelling, three bindings
}

TEST(reinserting_same_pair_is_idempotent) {
    IdentifierTrie t;
    t.insert("x", 1);
    t.insert("x", 1);
    CHECK(eq(t.findExact("x"), Ids{1}));
    CHECK(t.size() == 1);
}

TEST(missing_identifiers) {
    IdentifierTrie t;
    t.insert("counter", 1);
    CHECK(!t.contains("count"));    // proper prefix, not an identifier
    CHECK(!t.contains("counters")); // extension
    CHECK(!t.contains("zzz"));
    CHECK(t.findExact("count").empty());
    CHECK(t.findExact("zzz").empty());
}

TEST(case_sensitivity) {
    IdentifierTrie t;
    t.insert("Foo", 1);
    t.insert("foo", 2);
    t.insert("FOO", 3);
    CHECK(eq(t.findExact("Foo"), Ids{1}));
    CHECK(eq(t.findExact("foo"), Ids{2}));
    CHECK(eq(t.findExact("FOO"), Ids{3}));
    CHECK(!t.contains("fOO"));
    CHECK(eq(t.findByPrefix("F", 10), Strings{"FOO", "Foo"}));
    CHECK(eq(t.findByPrefix("f", 10), Strings{"foo"}));
}

TEST(identifier_characters) {
    IdentifierTrie t;
    t.insert("_tmp0", 1);
    t.insert("a_b_9", 2);
    CHECK(t.contains("_tmp0"));
    CHECK(t.contains("a_b_9"));
    // Non-ASCII bytes are stored verbatim (and sort as unsigned bytes).
    t.insert("caf\xC3\xA9", 3);
    CHECK(eq(t.findExact("caf\xC3\xA9"), Ids{3}));
    CHECK(eq(t.findByPrefix("caf", 5), Strings{"caf\xC3\xA9"}));
}

// ----------------------------- empty input ---------------------------------

TEST(empty_input_is_safe) {
    IdentifierTrie t;
    CHECK(t.empty());
    t.insert("", 1);  // ignored
    CHECK(t.empty());
    CHECK(!t.contains(""));
    CHECK(t.findExact("").empty());
    CHECK(t.findByPrefix("", 10).empty());
    CHECK(t.suggest("", 10).empty());
    CHECK(t.snapshot().empty());

    t.insert("a", 1);
    t.insert("bb", 2);
    CHECK(!t.contains(""));
    CHECK(t.findExact("").empty());
    CHECK(eq(t.findByPrefix("", 10), Strings{"a", "bb"}));  // empty prefix = all
    CHECK(eq(t.suggest("", 10), Strings{"a", "bb"}));       // distances 1, 2
}

// ----------------------------- prefix search -------------------------------

TEST(shared_prefixes_sorted_and_include_prefix_itself) {
    IdentifierTrie t;
    for (const char* s : {"get", "getB", "getA", "gettext", "getter", "set", "ge"})
        t.insert(s, 1);
    CHECK(eq(t.findByPrefix("get", 100),
             Strings{"get", "getA", "getB", "getter", "gettext"}));
    CHECK(eq(t.findByPrefix("ge", 100),
             Strings{"ge", "get", "getA", "getB", "getter", "gettext"}));
    CHECK(eq(t.findByPrefix("gett", 100), Strings{"getter", "gettext"}));
    CHECK(t.findByPrefix("x", 100).empty());
    CHECK(t.findByPrefix("getz", 100).empty());
}

TEST(prefix_that_is_not_terminal_is_not_returned) {
    IdentifierTrie t;
    t.insert("counter", 1);
    CHECK(eq(t.findByPrefix("count", 10), Strings{"counter"}));
}

TEST(prefix_result_limit) {
    IdentifierTrie t;
    for (const char* s : {"a1", "a2", "a3", "a4", "a5"}) t.insert(s, 1);
    CHECK(t.findByPrefix("a", 0).empty());
    CHECK(eq(t.findByPrefix("a", 1), Strings{"a1"}));
    CHECK(eq(t.findByPrefix("a", 3), Strings{"a1", "a2", "a3"}));
    CHECK(t.findByPrefix("a", 99).size() == 5);
}

// ------------------------------ suggestions --------------------------------

TEST(suggest_orders_by_distance_then_lexicographic) {
    IdentifierTrie t;
    for (const char* s : {"count", "mount", "counts", "cost", "total", "court"})
        t.insert(s, 1);
    // target "count": count=0; mount/counts/court=1; cost=1 (delete 'u'? c-o-s-t
    // vs c-o-u-n-t: delete u + substitute n->s = 2).
    CHECK(eq(t.suggest("count", 10),
             Strings{"count", "counts", "court", "mount", "cost"}));
}

TEST(suggest_typical_typos) {
    IdentifierTrie t;
    for (const char* s : {"counter", "index", "length", "buffer"}) t.insert(s, 1);
    CHECK(eq(t.suggest("conter", 3), Strings{"counter"}));   // deletion
    CHECK(eq(t.suggest("indax", 3), Strings{"index"}));      // substitution
    CHECK(eq(t.suggest("lenngth", 3), Strings{"length"}));   // insertion
    CHECK(eq(t.suggest("bufefr", 3), Strings{"buffer"}));    // transposition = 2
}

TEST(suggest_respects_max_distance_and_default) {
    IdentifierTrie t;
    t.insert("abcdef", 1);
    CHECK(t.suggest("abcxyz", 5).empty());        // distance 3 > default 2
    CHECK(eq(t.suggest("abcxyz", 5, 3), Strings{"abcdef"}));
    CHECK(t.suggest("abcdxf", 5, 0).empty());     // only exact allowed
    CHECK(eq(t.suggest("abcdef", 5, 0), Strings{"abcdef"}));
}

TEST(suggest_limit_keeps_best_deterministically) {
    IdentifierTrie t;
    for (const char* s : {"ab", "ac", "ad", "ae", "abc"}) t.insert(s, 1);
    CHECK(t.suggest("ab", 0).empty());
    CHECK(eq(t.suggest("ab", 1), Strings{"ab"}));                // distance 0
    CHECK(eq(t.suggest("ab", 3), Strings{"ab", "abc", "ac"}));   // then d=1, lex
    CHECK(eq(t.suggest("ab", 99, 1), Strings{"ab", "abc", "ac", "ad", "ae"}));
}

TEST(suggest_is_case_sensitive) {
    IdentifierTrie t;
    t.insert("Foo", 1);
    CHECK(eq(t.suggest("foo", 5, 1), Strings{"Foo"}));  // 1 substitution
    CHECK(t.suggest("foo", 5, 0).empty());
}

TEST(suggest_on_empty_trie) {
    IdentifierTrie t;
    CHECK(t.suggest("abc", 5).empty());
}

// Differential test against a brute-force scan, including small limits so the
// top-k pruning path is exercised.
TEST(suggest_matches_bruteforce_on_random_data) {
    std::mt19937 rng(12345);
    const std::string alphabet = "abc_";
    auto randomWord = [&](std::size_t maxLen) {
        std::uniform_int_distribution<std::size_t> len(1, maxLen);
        std::uniform_int_distribution<std::size_t> ch(0, alphabet.size() - 1);
        std::string s(len(rng), 'a');
        for (char& c : s) c = alphabet[ch(rng)];
        return s;
    };

    for (int round = 0; round < 40; ++round) {
        IdentifierTrie t;
        std::set<std::string> words;
        for (int i = 0; i < 60; ++i) {
            std::string w = randomWord(7);
            t.insert(w, static_cast<SymbolId>(i));
            words.insert(w);
        }
        CHECK(t.size() == words.size());

        for (int q = 0; q < 25; ++q) {
            const std::string target = randomWord(8);
            for (std::size_t maxDist : {0u, 1u, 2u, 3u}) {
                std::vector<std::pair<std::size_t, std::string>> expected;
                for (const auto& w : words) {
                    const std::size_t d = levenshtein(target, w);
                    if (d <= maxDist) expected.emplace_back(d, w);
                }
                std::sort(expected.begin(), expected.end());
                for (std::size_t limit : {1u, 3u, 8u, 1000u}) {
                    Strings want;
                    for (std::size_t i = 0; i < expected.size() && i < limit; ++i)
                        want.push_back(expected[i].second);
                    if (!eq(t.suggest(target, limit, maxDist), want)) {
                        ++g_failures;
                        std::cerr << "  mismatch: target=" << target
                                  << " maxDist=" << maxDist << " limit=" << limit
                                  << "\n";
                    }
                }
            }
        }
    }
}

// ------------------------- snapshot / ownership ----------------------------

TEST(snapshot_is_ordered_copy) {
    IdentifierTrie t;
    t.insert("b", 2);
    t.insert("a", 1);
    t.insert("a", 5);
    t.insert("ab", 3);
    auto snap = t.snapshot();
    CHECK(snap.size() == 3);
    CHECK(snap[0].spelling == "a" && eq(snap[0].symbols, Ids{1, 5}));
    CHECK(snap[1].spelling == "ab" && eq(snap[1].symbols, Ids{3}));
    CHECK(snap[2].spelling == "b" && eq(snap[2].symbols, Ids{2}));

    snap[0].symbols.push_back(99);  // mutating the copy must not affect the trie
    CHECK(eq(t.findExact("a"), Ids{1, 5}));
}

TEST(move_semantics) {
    IdentifierTrie a;
    a.insert("x", 1);
    IdentifierTrie b(std::move(a));
    CHECK(b.contains("x"));
    CHECK(a.empty());          // moved-from is valid and empty
    a.insert("y", 2);          // ...and reusable
    CHECK(a.contains("y"));

    IdentifierTrie c;
    c = std::move(b);
    CHECK(c.contains("x"));
    CHECK(b.empty());
}

TEST(long_identifier) {
    IdentifierTrie t;
    const std::string longName(5000, 'q');
    t.insert(longName, 42);
    CHECK(eq(t.findExact(longName), Ids{42}));
    CHECK(t.findByPrefix("qq", 5).size() == 1);
}

int main() {
    for (auto& [name, fn] : registry()) {
        const int before = g_failures;
        fn();
        std::cout << (g_failures == before ? "[ PASS ] " : "[ FAIL ] ") << name
                  << "\n";
    }
    std::cout << (g_failures == 0 ? "\nAll tests passed.\n"
                                  : "\nSome tests FAILED.\n");
    return g_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
