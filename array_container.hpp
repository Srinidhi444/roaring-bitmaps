
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

class ArrayContainer {
private:
    std::vector<uint16_t> values_;

public:
    // Insert a value while maintaining sorted order.
    // Returns true if inserted, false if already present.
    bool add(uint16_t value) {
        auto it = std::lower_bound(
            values_.begin(), values_.end(), value
        );

        if (it != values_.end() && *it == value) {
            return false;
        }

        values_.insert(it, value);
        return true;
    }

    // Remove a value if it exists.
    bool remove(uint16_t value) {
        auto it = std::lower_bound(
            values_.begin(), values_.end(), value
        );

        if (it == values_.end() || *it != value) {
            return false;
        }

        values_.erase(it);
        return true;
    }

    // Check whether a value exists.
    bool contains(uint16_t value) const {
        return std::binary_search(
            values_.begin(), values_.end(), value
        );
    }

    // Number of stored integers.
    std::size_t cardinality() const {
        return values_.size();
    }

    // Approximate payload size, excluding vector overhead.
    std::size_t memory_usage() const {
        return values_.capacity() * sizeof(uint16_t);
    }

    const std::vector<uint16_t>& values() const {
        return values_;
    }
};
