#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

class RunContainer {
public:
    struct Run { uint16_t start, length; }; // inclusive end = start + length
private:
    std::vector<Run> runs_;
    std::size_t cardinality_ = 0;
    static uint32_t end(const Run& r) { return uint32_t(r.start) + r.length; }
public:
    bool add(uint16_t v) {
        auto it = std::lower_bound(runs_.begin(), runs_.end(), v,
            [](const Run& r, uint16_t x) { return end(r) < x; });
        if (it != runs_.end() && v >= it->start) return false;
        const bool prev = it != runs_.begin() && end(*(it - 1)) + 1 == v;
        const bool next = it != runs_.end() && uint32_t(v) + 1 == it->start;
        if (prev && next) {
            (it - 1)->length = static_cast<uint16_t>(end(*it) - (it - 1)->start);
            runs_.erase(it);
        } else if (prev) {
            ++(it - 1)->length;
        } else if (next) {
            --it->start;
            ++it->length;
        } else {
            runs_.insert(it, Run{v, 0});
        }
        ++cardinality_;
        return true;
    }
    bool remove(uint16_t v) {
        auto it = std::lower_bound(runs_.begin(), runs_.end(), v,
            [](const Run& r, uint16_t x) { return end(r) < x; });
        if (it == runs_.end() || v < it->start) return false;
        const uint32_t last = end(*it);
        if (!it->length) runs_.erase(it);
        else if (v == it->start) { ++it->start; --it->length; }
        else if (v == last) --it->length;
        else {
            const std::size_t pos = static_cast<std::size_t>(it - runs_.begin());
            const Run right{static_cast<uint16_t>(uint32_t(v) + 1),
                            static_cast<uint16_t>(last - v - 1)};
            runs_.insert(runs_.begin() + pos + 1, right);
            runs_[pos].length = static_cast<uint16_t>(uint32_t(v) - runs_[pos].start - 1);
        }
        --cardinality_;
        return true;
    }
    bool contains(uint16_t v) const {
        auto it = std::lower_bound(runs_.begin(), runs_.end(), v,
            [](const Run& r, uint16_t x) { return end(r) < x; });
        return it != runs_.end() && v >= it->start;
    }
    void append_range(uint16_t first, uint16_t last) {
        if (first > last) throw std::invalid_argument("invalid range");
        if (!runs_.empty()) {
            const uint32_t prev_end = end(runs_.back());
            if (first <= prev_end) throw std::invalid_argument("overlapping run");
            if (first == prev_end + 1) {
                runs_.back().length = static_cast<uint16_t>(uint32_t(last) - runs_.back().start);
                cardinality_ += uint32_t(last) - first + 1;
                return;
            }
        }
        runs_.push_back({first, static_cast<uint16_t>(last - first)});
        cardinality_ += uint32_t(last) - first + 1;
    }
    std::size_t cardinality() const { return cardinality_; }
    std::size_t run_count() const { return runs_.size(); }
    std::size_t memory_usage() const { return runs_.capacity() * sizeof(Run); }
    const std::vector<Run>& runs() const { return runs_; }
    template <typename F> void for_each(F fn) const {
        for (const auto& run : runs_)
            for (uint32_t v = run.start; v <= end(run); ++v)
                fn(static_cast<uint16_t>(v));
    }
};
