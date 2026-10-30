#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>
#include <variant>
#include "array_container.hpp"
#include "bitmap_container.hpp"
#include "run_container.hpp"

class RoaringBitmap {
    static constexpr std::size_t LIMIT = 4096;
    using Container = std::variant<ArrayContainer, std::unique_ptr<BitmapContainer>, RunContainer>;
    std::map<uint16_t, Container> containers_;

    static uint16_t high(uint32_t x) { return static_cast<uint16_t>(x >> 16); }
    static uint16_t low(uint32_t x) { return static_cast<uint16_t>(x); }

    template <typename T> static decltype(auto) object(T& x) {
        if constexpr (std::is_same_v<std::remove_cv_t<T>, std::unique_ptr<BitmapContainer>>) return *x;
        else return (x);
    }
    template <typename F> static decltype(auto) visit(Container& c, F f) {
        return std::visit([&](auto& x) -> decltype(auto) { return f(object(x)); }, c);
    }
    template <typename F> static decltype(auto) visit(const Container& c, F f) {
        return std::visit([&](const auto& x) -> decltype(auto) { return f(object(x)); }, c);
    }
    template <typename T, typename F> static void values(const T& x, F f) {
        if constexpr (std::is_same_v<T, ArrayContainer>) {
            for (auto v : x.values()) f(v);
        } else x.for_each(f);
    }
    static Container clone(const Container& c) {
        return visit(c, [](const auto& x) -> Container {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, BitmapContainer>)
                return std::make_unique<BitmapContainer>(x);
            else return x;
        });
    }
    static BitmapContainer make_bitmap(const ArrayContainer& a) {
        BitmapContainer b;
        for (auto v : a.values()) b.add(v);
        return b;
    }
    static ArrayContainer make_array(const BitmapContainer& b) {
        ArrayContainer a;
        b.for_each([&](uint16_t v) { a.add(v); });
        return a;
    }
    static Container compact(BitmapContainer b) {
        if (b.cardinality() <= LIMIT) return make_array(b);
        return std::make_unique<BitmapContainer>(std::move(b));
    }
    static BitmapContainer make_run_bitmap(const RunContainer& r) {
        BitmapContainer b;
        for (auto run : r.runs())
            b.add_range(run.start, static_cast<uint16_t>(uint32_t(run.start) + run.length));
        return b;
    }
    static Container combine(const Container& a, const Container& b, bool is_and) {
        auto* ab = std::get_if<std::unique_ptr<BitmapContainer>>(&a);
        auto* bb = std::get_if<std::unique_ptr<BitmapContainer>>(&b);
        auto* ar = std::get_if<RunContainer>(&a);
        auto* br = std::get_if<RunContainer>(&b);

        if (ab && bb) {
            return compact(is_and ? (**ab).bit_and(**bb) : (**ab).bit_or(**bb));
        }
        if (ar && bb) {
            auto rb = make_run_bitmap(*ar);
            return compact(is_and ? rb.bit_and(**bb) : rb.bit_or(**bb));
        }
        if (ab && br) {
            auto rb = make_run_bitmap(*br);
            return compact(is_and ? (**ab).bit_and(rb) : (**ab).bit_or(rb));
        }
        if (ar && br) {
            RunContainer out;
            const auto& left = ar->runs();
            const auto& right = br->runs();
            if (is_and) {
                std::size_t i = 0, j = 0;
                while (i < left.size() && j < right.size()) {
                    uint32_t ae = uint32_t(left[i].start) + left[i].length;
                    uint32_t be = uint32_t(right[j].start) + right[j].length;
                    uint32_t first = std::max<uint32_t>(left[i].start, right[j].start);
                    uint32_t last = std::min(ae, be);
                    if (first <= last)
                        out.append_range(static_cast<uint16_t>(first), static_cast<uint16_t>(last));
                    if (ae < be) ++i;
                    else ++j;
                }
            } else {
                std::vector<std::pair<uint16_t, uint16_t>> ranges;
                for (const auto& r : left)
                    ranges.emplace_back(r.start, static_cast<uint16_t>(uint32_t(r.start) + r.length));
                for (const auto& r : right)
                    ranges.emplace_back(r.start, static_cast<uint16_t>(uint32_t(r.start) + r.length));
                std::sort(ranges.begin(), ranges.end());
                if (!ranges.empty()) {
                    uint32_t first = ranges.front().first;
                    uint32_t last = ranges.front().second;
                    for (std::size_t i = 1; i < ranges.size(); ++i) {
                        if (ranges[i].first <= last + 1)
                            last = std::max<uint32_t>(last, ranges[i].second);
                        else {
                            out.append_range(static_cast<uint16_t>(first), static_cast<uint16_t>(last));
                            first = ranges[i].first;
                            last = ranges[i].second;
                        }
                    }
                    out.append_range(static_cast<uint16_t>(first), static_cast<uint16_t>(last));
                }
            }
            return out;
        }

        // All other combinations: enumerate the operands without expanding
        // a full 32-bit bitmap. Results are compacted into array/bitmap form.
        BitmapContainer out;
        if (is_and) {
            auto count_a = visit(a, [](const auto& x) { return x.cardinality(); });
            auto count_b = visit(b, [](const auto& x) { return x.cardinality(); });
            const Container& small = count_a <= count_b ? a : b;
            const Container& large = count_a <= count_b ? b : a;
            visit(small, [&](const auto& x) {
                values(x, [&](uint16_t v) {
                    if (visit(large, [&](const auto& y) { return y.contains(v); }))
                        out.add(v);
                });
            });
        } else {
            visit(a, [&](const auto& x) { values(x, [&](uint16_t v) { out.add(v); }); });
            visit(b, [&](const auto& x) { values(x, [&](uint16_t v) { out.add(v); }); });
        }
        return compact(std::move(out));
    }

public:
    RoaringBitmap() = default;
    RoaringBitmap(const RoaringBitmap& rhs) {
        for (const auto& [k,c] : rhs.containers_) containers_.emplace(k, clone(c));
    }
    RoaringBitmap& operator=(const RoaringBitmap& rhs) {
        if (this != &rhs) { RoaringBitmap tmp(rhs); containers_.swap(tmp.containers_); }
        return *this;
    }
    RoaringBitmap(RoaringBitmap&&) = default;
    RoaringBitmap& operator=(RoaringBitmap&&) = default;

    bool add(uint32_t v) {
        auto& c = containers_[high(v)];
        bool inserted = visit(c, [&](auto& x) { return x.add(low(v)); });
        if (inserted) {
            if (auto* a = std::get_if<ArrayContainer>(&c); a && a->cardinality() > LIMIT)
                c = std::make_unique<BitmapContainer>(make_bitmap(*a));
        }
        return inserted;
    }
    bool remove(uint32_t v) {
        auto it = containers_.find(high(v));
        if (it == containers_.end()) return false;
        auto& c = it->second;
        if (!visit(c, [&](auto& x) { return x.remove(low(v)); })) return false;
        auto count = visit(c, [](const auto& x) { return x.cardinality(); });
        if (count == 0) containers_.erase(it);
        else if (auto* b = std::get_if<std::unique_ptr<BitmapContainer>>(&c);
                 b && (*b)->cardinality() <= LIMIT) c = make_array(**b);
        return true;
    }
    bool contains(uint32_t v) const {
        auto it = containers_.find(high(v));
        return it != containers_.end() && visit(it->second, [&](const auto& x) { return x.contains(low(v)); });
    }
    std::size_t cardinality() const {
        std::size_t n=0;
        for (const auto& [k,c] : containers_) n += visit(c, [](const auto& x) { return x.cardinality(); });
        return n;
    }
    std::size_t container_count() const { return containers_.size(); }
    // Estimate includes C++ container objects and their owned buffers, plus approximate map-node bookkeeping.
    std::size_t estimated_memory_usage() const {
        std::size_t bytes = sizeof(*this);
        for (const auto& [key,c] : containers_) {
            bytes += sizeof(typename decltype(containers_)::value_type) + 3 * sizeof(void*);
            bytes += visit(c, [](const auto& x) -> std::size_t {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, BitmapContainer>) return x.memory_usage();
                else return x.memory_usage();
            });
        }
        return bytes;
    }
    void run_optimize() {
        for (auto& [key,c] : containers_) {
            if (std::holds_alternative<RunContainer>(c)) continue;
            RunContainer r;
            visit(c, [&](const auto& x) { values(x, [&](uint16_t v) { r.add(v); }); });
            const auto current = visit(c, [](const auto& x) -> std::size_t {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, BitmapContainer>) return 8192;
                else return x.cardinality() * sizeof(uint16_t);
            });
            if (2 + 4 * r.run_count() < current) c = std::move(r);
        }
    }
    RoaringBitmap intersection(const RoaringBitmap& rhs) const {
        RoaringBitmap out;
        for (const auto& [key,c] : containers_) {
            auto it=rhs.containers_.find(key);
            if (it==rhs.containers_.end()) continue;
            auto result=combine(c,it->second,true);
            if (visit(result, [](const auto& x){return x.cardinality()!=0;}))
                out.containers_.emplace(key,std::move(result));
        }
        return out;
    }
    RoaringBitmap unite(const RoaringBitmap& rhs) const {
        RoaringBitmap out(*this);
        for (const auto& [key,c] : rhs.containers_) {
            auto it=out.containers_.find(key);
            if (it==out.containers_.end()) out.containers_.emplace(key,clone(c));
            else it->second=combine(it->second,c,false);
        }
        return out;
    }
};
