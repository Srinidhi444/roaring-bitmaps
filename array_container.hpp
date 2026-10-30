#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

class ArrayContainer {
    std::vector<uint16_t> values_;
public:
    bool add(uint16_t v) {
        auto it = std::lower_bound(values_.begin(), values_.end(), v);
        if (it != values_.end() && *it == v) return false;
        values_.insert(it, v);
        return true;
    }
    bool remove(uint16_t v) {
        auto it = std::lower_bound(values_.begin(), values_.end(), v);
        if (it == values_.end() || *it != v) return false;
        values_.erase(it);
        return true;
    }
    bool contains(uint16_t v) const {
        return std::binary_search(values_.begin(), values_.end(), v);
    }
    std::size_t cardinality() const { return values_.size(); }
    std::size_t memory_usage() const { return values_.capacity() * sizeof(uint16_t); }
    const std::vector<uint16_t>& values() const { return values_; }
};
