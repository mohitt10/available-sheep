// Shared IDs used across components.
//
// SymbolId belongs to the Trie/frontend contract. ExprId and TypeId are
// currently defined inside expression_dag.hpp (provisional, see the DAG
// README); their owner may move them here later. There must be exactly one
// alias per ID: never redefine SymbolId in another header.
#pragma once
#include <cstdint>

using SymbolId = std::uint32_t;

namespace available_sheep {
using ::SymbolId;
}  // namespace available_sheep