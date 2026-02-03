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


#include <xsimd/xsimd.hpp>


#include "VebCommon.hpp"

/* Node8:
 * Represents a van Emde Boas tree node for universe size up to 2^8.
 *
 * The layout of this node is as follows:
 *   - An array of 4 uint64_t words (256 bits) to represent the presence of elements in the universe.
 *
 * The purpose of this design is to optimize memory usage while maintaining fast operations on the underlying nodes.
 */
struct Node8 {
    friend struct VebTree;
public:
    using subindex_t = std::uint8_t;
    using index_t = std::uint8_t;

private:
    using vec_type = xsimd::batch<std::uint64_t>;

    static constexpr int bits_per_word{std::numeric_limits<std::uint64_t>::digits};
    static constexpr int num_words{256 / bits_per_word};

    alignas(xsimd::default_arch::alignment()) std::array<std::uint64_t, num_words> bits_{};

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {x / bits_per_word, x % bits_per_word};
    }
    static constexpr index_t index(subindex_t word, subindex_t bit) {
        return static_cast<index_t>(word * bits_per_word + bit);
    }

    constexpr inline vec_type load() const {
        return vec_type::load_aligned(bits_.data());
    }
    constexpr inline void store(const vec_type &v) {
        v.store_aligned(bits_.data());
    }

    constexpr inline bool empty() const {
        return xsimd::all(load() == 0);
    }

    constexpr inline explicit Node8() = default;
    
public:
    static constexpr inline Node8 new_with(index_t x) {
        Node8 node{};
        node.insert(x);
        return node;
    }

    // Create a Node8 with all bits set except `x`.
    static constexpr inline Node8 new_all_but(index_t x) {
        Node8 node = new_with(x);
        node.not_inplace();
        return node;
    }

    // Create a Node8 with all bits set.
    static constexpr inline Node8 new_all() {
        Node8 node{};
        node.not_inplace();
        return node;
    }

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

        return empty();
    }

    constexpr inline bool contains(index_t x) const {
        const auto [word_idx, bit_idx] {decompose(x)};
        return (bits_[word_idx] & (1ULL << bit_idx)) != 0;
    }

    constexpr inline std::optional<index_t> successor(index_t x) const {
        const auto [start_word, start_bit] {decompose(x)};
        const std::uint64_t mask{~0ULL << (start_bit + 1)};

        std::uint64_t word{};
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
        if (x == 0) {
            return std::nullopt;
        }

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
        return std::popcount(bits_[0]) + std::popcount(bits_[1]) +
               std::popcount(bits_[2]) + std::popcount(bits_[3]);
    }

    // Serialization (32 bytes payload, little-endian u64 words)
    inline void serialize(std::string &out) const {
        for (std::uint64_t w : bits_) {
            write_u64(out, w);
        }
    }

    static inline Node8 deserialize(std::string_view buf, std::size_t &pos) {
        Node8 node{};
        for (std::size_t i = 0; i < num_words; ++i) {
            node.bits_[i] = read_u64(buf, pos);
        }
        return node;
    }

    // helper struct for count_range. allows passing either arg optionally
    struct count_range_args {
        index_t lo{static_cast<index_t>(0)};
        index_t hi{static_cast<index_t>(universe_size())};
    };
    constexpr inline std::size_t count_range(count_range_args args) const {
        const auto [lo, hi] {args};
        const auto [lw, lb] {decompose(lo)};
        const auto [hw, hb] {decompose(hi)};
        const auto lmask{~0ULL << lb};
        const auto hmask{~0ULL >> (bits_per_word - 1 - hb)};

        if (lw == hw) {
            return std::popcount(bits_[lw] & lmask & hmask);
        }

        auto acc{std::popcount(bits_[lw] & lmask) +
                 std::popcount(bits_[hw] & hmask)};
        for (int i = lw + 1; i < hw; ++i) {
            acc += std::popcount(bits_[i]);
        }
        return acc;
    }

    static constexpr inline VebTreeMemoryStats get_memory_stats() {
        return {0, 0, 1};
    }

    // Truth table for set operations:
    //
    //  A | B | -A | A U B | A & B | A ^ B | A \ B 
    // --------------------------------------------
    //  0 | 0 |  1 |   0   |   0   |   0   |   0
    //  0 | 1 |  1 |   1   |   0   |   1   |   0
    //  1 | 0 |  0 |   1   |   0   |   1   |   1
    //  1 | 1 |  0 |   1   |   1   |   0   |   0

    constexpr inline bool not_inplace() {
        auto v = load();
        store(~v);
        return empty();
    }

    constexpr inline bool or_inplace(const Node8& other) {
        auto v1 = load();
        auto v2 = other.load();
        store(v1 | v2);
        // no need to check for emptiness, or can only ever grow a set
        return false;
    }

    constexpr inline bool xor_inplace(const Node8& other) {
        auto v1 = load();
        auto v2 = other.load();
        store(v1 ^ v2);
        return empty();
    }

    constexpr inline bool and_inplace(const Node8& other) {
        auto v1 = load();
        auto v2 = other.load();
        store(v1 & v2);
        return empty();
    }

    // difference: A \ B
    constexpr inline bool andnot_inplace(const Node8& other) {
        auto v1 = load();
        auto v2 = other.load();
        store(xsimd::bitwise_andnot(v2, v1));
        return empty();
    }
};

#endif // NODE8_HPP
