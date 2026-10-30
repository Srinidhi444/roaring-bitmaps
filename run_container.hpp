
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

class RunContainer {
private:
    struct Run {
        uint16_t start;
        uint16_t length; // Last value = start + length
    };

    std::vector<Run> runs_;
    std::size_t cardinality_ = 0;

public:
    bool add(uint16_t value) {
        // Find the first run whose end >= value.
        auto it = std::lower_bound(
            runs_.begin(), runs_.end(), value,
            [](const Run& run, uint16_t v) {
                return uint32_t(run.start) + run.length < v;
            }
        );

        // Already present.
        if (it != runs_.end() && value >= it->start) {
            return false;
        }

        bool join_prev =
            it != runs_.begin() &&
            uint32_t((it - 1)->start) +
                (it - 1)->length + 1 == value;

        bool join_next =
            it != runs_.end() &&
            uint32_t(value) + 1 == it->start;

        if (join_prev && join_next) {
            // Merge the previous and next runs.
            auto& prev = *(it - 1);
            uint32_t end = uint32_t(it->start) + it->length;

            prev.length = static_cast<uint16_t>(
                end - prev.start
            );
            runs_.erase(it);
        } else if (join_prev) {
            // Extend previous run to the right.
            ++(it - 1)->length;
        } else if (join_next) {
            // Extend next run to the left.
            --it->start;
            ++it->length;
        } else {
            // Create a new one-element run.
            runs_.insert(it, Run{value, 0});
        }

        ++cardinality_;
        return true;
    }

    bool remove(uint16_t value) {
        auto it = std::lower_bound(
            runs_.begin(), runs_.end(), value,
            [](const Run& run, uint16_t v) {
                return uint32_t(run.start) + run.length < v;
            }
        );

        if (it == runs_.end() || value < it->start) {
            return false;
        }

        uint32_t end = uint32_t(it->start) + it->length;

        if (it->length == 0) {
            // Remove a run containing only one value.
            runs_.erase(it);
        } else if (value == it->start) {
            // Remove the first value.
            ++it->start;
            --it->length;
        } else if (value == end) {
            // Remove the last value.
            --it->length;
        } else {
            // Split into two runs.
            Run right{
                static_cast<uint16_t>(value + 1),
                static_cast<uint16_t>(end - value - 1)
            };

            it->length = static_cast<uint16_t>(
                uint32_t(value) - it->start - 1
            );

            runs_.insert(it + 1, right);
        }

        --cardinality_;
        return true;
    }

    bool contains(uint16_t value) const {
        auto it = std::lower_bound(
            runs_.begin(), runs_.end(), value,
            [](const Run& run, uint16_t v) {
                return uint32_t(run.start) + run.length < v;
            }
        );

        return it != runs_.end() && value >= it->start;
    }

    std::size_t cardinality() const {
        return cardinality_;
    }

    std::size_t run_count() const {
        return runs_.size();
    }

    std::size_t memory_usage() const {
        return runs_.capacity() * sizeof(Run);
    }
};
