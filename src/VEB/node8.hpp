#ifndef NODE8_HPP
#define NODE8_HPP

#include <algorithm> // std::ranges::all_of
#include <array>     // std::array
#include <bit>       // std::popcount, std::countr_zero, std::countl_zero
#include <cstddef>   // std::size_t
#include <cstdint>   // std::uint8_t, std::uint64_t
#include <limits>    // std::numeric_limits
#include <optional>  // std::nullopt, std::optional
#include <utility>   // std::pair, std::unreachable

#include "VebCommon.hpp"

class Node8 {
    friend class VebTree;
public:
    using subindex_t = std::uint8_t;
    using index_t = std::uint8_t;

private:
    std::array<std::uint64_t, 4> bits_ = {};

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {x >> 6, x & 63};
    }

    static constexpr index_t index(subindex_t word, subindex_t bit) {
        return static_cast<index_t>(word * 64 + bit);
    }

public:
    constexpr inline index_t get_cluster_index(index_t key) const {
        const auto [target_word, target_bit] = decompose(key);

        index_t count{0};

        for (subindex_t word = 0; word < target_word; ++word) {
            count += static_cast<index_t>(std::popcount(bits_[word]));
        }

        std::uint64_t mask{(1ULL << target_bit) - 1};
        count += static_cast<index_t>(std::popcount(bits_[target_word] & mask));

        return count;
    }

    constexpr inline explicit Node8(index_t x) {
        const auto [word_idx, bit_idx] = decompose(x);
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    static constexpr std::size_t universe_size() { return std::numeric_limits<index_t>::max(); }

    constexpr inline index_t min() const {
        for (subindex_t word = 0; word < 4; ++word) {
            if (bits_[word] != 0) {
                return index(word, static_cast<subindex_t>(std::countr_zero(bits_[word])));
            }
        }
        std::unreachable();
    }

    constexpr inline index_t max() const {
        for (subindex_t word = 4; word > 0; --word) {
            if (bits_[word - 1] != 0) {
                return index(word - 1, static_cast<subindex_t>(63 - std::countl_zero(bits_[word - 1])));
            }
        }
        std::unreachable();
    }

    constexpr inline void insert(index_t x) {
        const auto [word_idx, bit_idx] = decompose(x);
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    constexpr inline bool remove(index_t x) {
        const auto [word_idx, bit_idx] = decompose(x);

        if (!(bits_[word_idx] & (1ULL << bit_idx))) {
            return false;
        }

        bits_[word_idx] &= ~(1ULL << bit_idx);

        return std::ranges::all_of(bits_, [](std::uint64_t word) { return word == 0; });
    }

    constexpr inline bool contains(index_t x) const {
        const auto [word_idx, bit_idx] = decompose(x);
        return (bits_[word_idx] & (1ULL << bit_idx)) != 0;
    }

    constexpr inline std::optional<index_t> successor(index_t x) const {
        const auto [start_word, start_bit] = decompose(x);

        std::uint64_t word{0};
        if (start_bit + 1 < 64) {
            word = bits_[start_word] & (~0ULL << (start_bit + 1));
        }
        if (word != 0) {
            return index(start_word, static_cast<subindex_t>(std::countr_zero(word)));
        }

        for (subindex_t word_idx = start_word + 1; word_idx < 4; ++word_idx) {
            if (bits_[word_idx] != 0) {
                return index(word_idx, static_cast<subindex_t>(std::countr_zero(bits_[word_idx])));
            }
        }

        return std::nullopt;
    }

    constexpr inline std::optional<index_t> predecessor(index_t x) const {
        const auto [start_word, start_bit] = decompose(x - 1);

        std::uint64_t word = bits_[start_word] & (start_bit == 63 ? -1ULL : ((1ULL << (start_bit + 1)) - 1));
        if (word != 0) {
            return index(start_word, static_cast<subindex_t>(63 - std::countl_zero(word)));
        }

        for (subindex_t word_idx = start_word; word_idx > 0; --word_idx) {
            if (bits_[word_idx - 1] != 0) {
                return index(word_idx - 1, static_cast<subindex_t>(63 - std::countl_zero(bits_[word_idx - 1])));
            }
        }

        return std::nullopt;
    }

    constexpr inline std::size_t size() const {
        return std::popcount(bits_[0]) + std::popcount(bits_[1]) +
               std::popcount(bits_[2]) + std::popcount(bits_[3]);
    }

    constexpr VebTreeMemoryStats get_memory_stats() const {
        return {0, 0, 1};
    }

    Node8 clone() const {
        return *this;
    }

    void destroy() {}

    bool is_tombstone() const {
        return size() == 0;
    }

    Node8& not_inplace() {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] = ~bits_[i];
        }
        return *this;
    }
    Node8& and_inplace(const Node8& other) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] &= other.bits_[i];
        }
        return *this;
    }
    Node8& and_not_inplace(const Node8& other) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] &= ~other.bits_[i];
        }
        return *this;
    }
    Node8& or_inplace(const Node8& other) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] |= other.bits_[i];
        }
        return *this;
    }
    Node8& xor_inplace(const Node8& other) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] ^= other.bits_[i];
        }
        return *this;
    }
    Node8& diff_inplace(const Node8& other) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] = (bits_[i] | other.bits_[i]) & ~(bits_[i] & other.bits_[i]);
        }
        return *this;
    }
};

#endif // NODE8_HPP
