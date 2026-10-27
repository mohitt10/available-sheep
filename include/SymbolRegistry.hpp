#pragma once
#include <cstddef>
#include <map>
#include <string>
#include "shared_types.hpp"
 
class SymbolRegistry {
public:
    virtual ~SymbolRegistry() = default;
    virtual SymbolId idFor(const std::string& name) = 0;
};
 
class SimpleSymbolRegistry : public SymbolRegistry {
public:
    SymbolId idFor(const std::string& name) override {
        auto it = ids.find(name);
        if (it != ids.end()) return it->second;
        SymbolId id = next++;
        ids[name] = id;
        return id;
    }
    std::size_t size() const { return ids.size(); }
 
private:
    std::map<std::string, SymbolId> ids;
    SymbolId next = 1;
};
 