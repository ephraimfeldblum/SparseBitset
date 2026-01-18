#ifndef NODE8_HPP
#define NODE8_HPP

#include <algorithm> // std::ranges::all_of
#include <array>     // std::array
#include <bit>       // std::popcount, std::countr_zero, std::countl_zero
#include <cstddef>   // std::size_t
#include <cstdint>   // std::uint8_t, std::uint64_t
#include <limits>    // std::numeric_limits
#include <numeric>   // std::reduce
#include <optional>  // std::nullopt, std::optional
#include <utility>   // std::pair, std::unreachable

#include "VebCommon.hpp"

/* Node8:
 * Represents a van Emde Boas tree node for universe size up to 2^8.
 *
 * The layout of this node is as follows:
 *   - An array of 4 uint64_t words (256 bits) to represent the presence of elements in the universe.
 *
 * The purpose of this design is to optimize memory usage while maintaining fast operations on the underlying nodes.
 */
class Node8 {
    friend class VebTree;
public:
    using subindex_t = std::uint8_t;
    using index_t = std::uint8_t;

private:
    static constexpr int bits_per_word{std::numeric_limits<std::uint64_t>::digits};
    static constexpr int num_words{256 / bits_per_word};

    std::array<std::uint64_t, num_words> bits_ = {};

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {x / bits_per_word, x % bits_per_word};
    }
    static constexpr index_t index(subindex_t word, subindex_t bit) {
        return static_cast<index_t>(word * bits_per_word + bit);
    }
    
public:
    constexpr inline index_t get_cluster_index(index_t key) const {
        const auto [target_word, target_bit] {decompose(key)};
        const std::uint64_t mask{(1ULL << target_bit) - 1};

        index_t count{0};
        for (subindex_t word{}; word < target_word; ++word) {
            count += static_cast<index_t>(std::popcount(bits_[word]));
        }
        count += static_cast<index_t>(std::popcount(bits_[target_word] & mask));
        return count;
    }

    constexpr inline explicit Node8(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};
        bits_[word_idx] |= (1ULL << bit_idx);
    }
    constexpr inline explicit Node8(const std::array<std::uint64_t, num_words>& bits)
        : bits_{bits} {
    }

    constexpr inline Node8(const Node8& other) = delete;
    constexpr inline Node8(Node8&& other) noexcept = default;
    constexpr inline Node8& operator=(const Node8&) = delete;
    constexpr inline Node8& operator=(Node8&&) noexcept = default;
    constexpr inline ~Node8() = default;

    static constexpr inline std::size_t universe_size() {
        return std::numeric_limits<index_t>::max();
    }

    constexpr inline index_t min() const {
        for (subindex_t word{}; word < num_words; ++word) {
            if (bits_[word] != 0) {
                auto bit_idx{std::countr_zero(bits_[word])};
                return index(word, static_cast<subindex_t>(bit_idx));
            }
        }
        std::unreachable();
    }

    constexpr inline index_t max() const {
        for (subindex_t word{num_words}; word > 0; --word) {
            if (bits_[word - 1] != 0) {
                auto bit_idx{bits_per_word - 1 - std::countl_zero(bits_[word - 1])};
                return index(word - 1, static_cast<subindex_t>(bit_idx));
            }
        }
        std::unreachable();
    }

    constexpr inline void insert(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    constexpr inline bool remove(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};

        if (!(bits_[word_idx] & (1ULL << bit_idx))) {
            return false;
        }
        bits_[word_idx] &= ~(1ULL << bit_idx);

        return std::ranges::all_of(bits_, [](std::uint64_t word) { return word == 0; });
    }

    constexpr inline bool contains(index_t x) const {
        const auto [word_idx, bit_idx] {decompose(x)};
        return (bits_[word_idx] & (1ULL << bit_idx)) != 0;
    }

    constexpr inline std::optional<index_t> successor(index_t x) const {
        const auto [start_word, start_bit] {decompose(x)};
        const std::uint64_t mask{~0ULL << (start_bit + 1)};

        std::uint64_t word{0};
        if (start_bit + 1 < bits_per_word) {
            word = bits_[start_word] & mask;
        }
        if (word != 0) {
            auto bit_idx{std::countr_zero(word)};
            return index(start_word, static_cast<subindex_t>(bit_idx));
        }

        for (auto word_idx{start_word + 1}; word_idx < num_words; ++word_idx) {
            if (bits_[word_idx] != 0) {
                auto bit_idx{std::countr_zero(bits_[word_idx])};
                return index(static_cast<subindex_t>(word_idx), static_cast<subindex_t>(bit_idx));
            }
        }

        return std::nullopt;
    }

    constexpr inline std::optional<index_t> predecessor(index_t x) const {
        const auto [start_word, start_bit] {decompose(x - 1)};
        const auto mask{start_bit == bits_per_word - 1 ? -1ULL : ((1ULL << (start_bit + 1)) - 1)};

        if (std::uint64_t word{bits_[start_word] & mask}; word != 0) {
            auto bit_idx{bits_per_word - 1 - std::countl_zero(word)};
            return index(start_word, static_cast<subindex_t>(bit_idx));
        }

        for (subindex_t word_idx{start_word}; word_idx > 0; --word_idx) {
            if (bits_[word_idx - 1] != 0) {
                auto bit_idx{bits_per_word - 1 - std::countl_zero(bits_[word_idx - 1])};
                return index(word_idx - 1, static_cast<subindex_t>(bit_idx));
            }
        }

        return std::nullopt;
    }

    constexpr inline std::size_t size() const {
        return std::reduce(
#if __cpp_lib_execution
            std::execution::unseq,
#endif
            bits_.cbegin(), bits_.cend(), 0uz,
            [](std::size_t acc, std::uint64_t word) { return acc + std::popcount(word); }
        );
    }

    constexpr inline VebTreeMemoryStats get_memory_stats() const {
        return {0, 0, 1};
    }

    constexpr inline Node8 clone() const {
        return Node8{bits_};
    }

    constexpr inline void destroy() {}

    constexpr inline bool is_tombstone() const {
        return size() == 0;
    }

    constexpr inline decltype(auto) not_inplace(this auto&& self) {
        for (std::size_t i{}; i < num_words; ++i) {
            self.bits_[i] = ~self.bits_[i];
        }
        return std::forward<decltype(self)>(self);
    }
    constexpr inline decltype(auto) and_inplace(this auto&& self, const Node8& other) {
        for (std::size_t i{}; i < num_words; ++i) {
            self.bits_[i] &= other.bits_[i];
        }
        return std::forward<decltype(self)>(self);
    }
    constexpr inline decltype(auto) and_not_inplace(this auto&& self, const Node8& other) {
        for (std::size_t i{}; i < num_words; ++i) {
            self.bits_[i] &= ~other.bits_[i];
        }
        return std::forward<decltype(self)>(self);
    }
    constexpr inline decltype(auto) or_inplace(this auto&& self, const Node8& other) {
        for (std::size_t i{}; i < num_words; ++i) {
            self.bits_[i] |= other.bits_[i];
        }
        return std::forward<decltype(self)>(self);
    }
    constexpr inline decltype(auto) xor_inplace(this auto&& self, const Node8& other) {
        for (std::size_t i{}; i < num_words; ++i) {
            self.bits_[i] ^= other.bits_[i];
        }
        return std::forward<decltype(self)>(self);
    }
    constexpr inline decltype(auto) diff_inplace(this auto&& self, const Node8& other) {
        for (std::size_t i{}; i < num_words; ++i) {
            self.bits_[i] = (self.bits_[i] | other.bits_[i]) & ~(self.bits_[i] & other.bits_[i]);
        }
        return std::forward<decltype(self)>(self);
    }
};

static_assert(sizeof(Node8) == 32, "Node8 size is incorrect");
static_assert(std::is_standard_layout_v<Node8>, "Node8 must be standard layout");
static_assert(std::is_trivially_destructible_v<Node8>, "Node8 must be trivially destructible");
static_assert(std::is_move_constructible_v<Node8>, "Node8 must be move constructible");
static_assert(std::is_trivially_move_constructible_v<Node8>, "Node8 must be trivially move constructible");
static_assert(std::is_nothrow_move_constructible_v<Node8>, "Node8 must be no throw move constructible");
static_assert(std::is_move_assignable_v<Node8>, "Node8 must be move assignable");
static_assert(std::is_trivially_move_assignable_v<Node8>, "Node8 must be trivially move assignable");
static_assert(std::is_nothrow_move_assignable_v<Node8>, "Node8 must be no throw move assignable");

#endif // NODE8_HPP
