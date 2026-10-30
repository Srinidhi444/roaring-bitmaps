#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

class BitmapContainer {
    static constexpr std::size_t WORDS = 1024;
    std::array<uint64_t, WORDS> bits_{};
    std::size_t cardinality_ = 0;
public:
    bool add(uint16_t v) {
        auto& word = bits_[v / 64];
        const uint64_t mask = uint64_t{1} << (v % 64);
        if (word & mask) return false;
        word |= mask;
        ++cardinality_;
        return true;
    }
    bool remove(uint16_t v) {
        auto& word = bits_[v / 64];
        const uint64_t mask = uint64_t{1} << (v % 64);
        if (!(word & mask)) return false;
        word &= ~mask;
        --cardinality_;
        return true;
    }
    bool contains(uint16_t v) const {
        return (bits_[v / 64] & (uint64_t{1} << (v % 64))) != 0;
    }
    std::size_t cardinality() const { return cardinality_; }
    std::size_t memory_usage() const { return sizeof(*this); }
    std::size_t count_bits() const {
        std::size_t n = 0;
        for (auto w : bits_) n += std::popcount(w);
        return n;
    }
    BitmapContainer bit_and(const BitmapContainer& b) const {
        BitmapContainer out;
        for (std::size_t i = 0; i < WORDS; ++i) {
            out.bits_[i] = bits_[i] & b.bits_[i];
            out.cardinality_ += std::popcount(out.bits_[i]);
        }
        return out;
    }
    BitmapContainer bit_or(const BitmapContainer& b) const {
        BitmapContainer out;
        for (std::size_t i = 0; i < WORDS; ++i) {
            out.bits_[i] = bits_[i] | b.bits_[i];
            out.cardinality_ += std::popcount(out.bits_[i]);
        }
        return out;
    }
    // Bit-mask operations directly on a contiguous range (inclusive).
    void add_range(uint16_t first, uint16_t last) {
        if (first > last) return;
        const std::size_t a = first / 64, b = last / 64;
        for (std::size_t i = a; i <= b; ++i) {
            const unsigned lo = i == a ? first % 64 : 0;
            const unsigned hi = i == b ? last % 64 : 63;
            const uint64_t lower = ~uint64_t{0} << lo;
            const uint64_t upper = hi == 63 ? ~uint64_t{0} : (uint64_t{1} << (hi + 1)) - 1;
            const uint64_t mask = lower & upper;
            const uint64_t old = bits_[i];
            bits_[i] |= mask;
            cardinality_ += std::popcount(bits_[i]) - std::popcount(old);
        }
    }
    // Intersection with a run, visiting only the intersecting words.
    void keep_range(uint16_t first, uint16_t last) {
        BitmapContainer temp;
        temp.add_range(first, last);
        *this = bit_and(temp);
    }
    template <typename F> void for_each(F fn) const {
        for (std::size_t i = 0; i < WORDS; ++i) {
            uint64_t word = bits_[i];
            while (word) {
                const unsigned bit = std::countr_zero(word);
                fn(static_cast<uint16_t>(i * 64 + bit));
                word &= word - 1;
            }
        }
    }
};
