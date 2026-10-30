
#pragma once

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

class BitmapContainer {
private:
    static constexpr std::size_t WORD_COUNT = 1024;

    std::array<uint64_t, WORD_COUNT> bits_{};
    std::size_t cardinality_ = 0;

public:
    // Set the bit corresponding to value.
    // Returns false if the value already exists.
    bool add(uint16_t value) {
        std::size_t word = value / 64;
        uint64_t mask = uint64_t{1} << (value % 64);

        if ((bits_[word] & mask) != 0) {
            return false;
        }

        bits_[word] |= mask;
        ++cardinality_;
        return true;
    }

    // Clear the bit corresponding to value.
    bool remove(uint16_t value) {
        std::size_t word = value / 64;
        uint64_t mask = uint64_t{1} << (value % 64);

        if ((bits_[word] & mask) == 0) {
            return false;
        }

        bits_[word] &= ~mask;
        --cardinality_;
        return true;
    }

    // Check whether a bit is set.
    bool contains(uint16_t value) const {
        std::size_t word = value / 64;
        uint64_t mask = uint64_t{1} << (value % 64);

        return (bits_[word] & mask) != 0;
    }

    std::size_t cardinality() const {
        return cardinality_;
    }

    std::size_t memory_usage() const {
        return sizeof(bits_);
    }

    // Count all set bits directly from the bitmap.
    std::size_t count_bits() const {
        std::size_t count = 0;

        for (uint64_t word : bits_) {
            count += std::popcount(word);
        }

        return count;
    }
};
