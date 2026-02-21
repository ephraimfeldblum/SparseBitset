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

    struct Iterator {
        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = index_t;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = index_t;

        std::uintptr_t data;
        
        static constexpr const Node8* canonicalize(std::uintptr_t x) {
            x &= 0x0000FFFFFFFFFFFF;
            // The canonical form of a pointer is with bits 48..63 set to whatever bit 47 is, to allow for both zero-extension and sign-extension of the pointer value.
            if (x & 0x0000'8000'0000'0000) {
                x |= 0xFFFF'0000'0000'0000;
            }
            return reinterpret_cast<const Node8*>(x);
        }

        static constexpr std::pair<std::uint16_t, const Node8*> decompose(std::uintptr_t x) {
            return {x >> 48, canonicalize(x)};
        }

        static constexpr std::uintptr_t compose(const Node8* node, std::uint16_t value) {
            return (static_cast<std::uintptr_t>(value) << 48) | (reinterpret_cast<std::uintptr_t>(node) & 0x0000FFFFFFFFFFFF);
        }

        static constexpr Iterator create(const Node8* node, std::uint16_t value) {
            return Iterator{compose(node, value)};
        }

        static constexpr Iterator sentinel(const Node8* node = nullptr) {
            return Iterator{compose(node, UINT16_MAX)};
        }

        constexpr bool is_sentinel() const {
            const auto [value, _] = decompose(data);
            return value == UINT16_MAX;
        }

        constexpr Iterator& operator++() {
            const auto [value, node] = decompose(data);
            if (is_sentinel()) {
                *this = node->min();
                return *this;
            }

            const auto succ = node->successor(static_cast<index_t>(value));
            data = compose(node, succ.is_sentinel() ? UINT16_MAX : *succ);
            return *this;
        }

        constexpr Iterator operator++(int) {
            Iterator tmp{*this};
            ++*this;
            return tmp;
        }

        constexpr Iterator& operator--() {
            const auto [value, node] = decompose(data);
            if (is_sentinel()) {
                *this = node->max();
                return *this;
            }

            const auto pred = node->predecessor(static_cast<index_t>(value));
            if (pred.is_sentinel()) {
                data = compose(node, UINT16_MAX);
                return *this;
            }
            data = compose(node, *pred);
            return *this;
        }

        constexpr Iterator operator--(int) {
            Iterator tmp{*this};
            --*this;
            return tmp;
        }

        constexpr bool operator==(Iterator other) const {
            if (is_sentinel() && other.is_sentinel()) {
                return true;
            }
            if (is_sentinel() || other.is_sentinel()) {
                return false;
            }
            return data == other.data;
        }

        constexpr bool operator!=(Iterator other) const {
            return !(*this == other);
        }

        constexpr reference operator*() const {
            const auto [value, _] = decompose(data);
            return static_cast<index_t>(value);
        }
    };

private:
    static constexpr int bits_per_word{std::numeric_limits<std::uint64_t>::digits};
    static constexpr int num_words{256uz / bits_per_word};

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {x / bits_per_word, x % bits_per_word};
    }
    static constexpr index_t index(subindex_t word, subindex_t bit) {
        return static_cast<index_t>(word * bits_per_word + bit);
    }

    using vec_type = xsimd::make_sized_batch_t<std::uint64_t, num_words>;
    alignas(num_words * sizeof(std::uint64_t))
    std::array<std::uint64_t, num_words> bits{};

    constexpr vec_type load() const {
        return vec_type::load_aligned(bits.data());
    }
    constexpr void store(const vec_type &v) {
        v.store_aligned(bits.data());
    }

    constexpr bool empty() const {
        return xsimd::all(load() == 0);
    }

    constexpr explicit Node8() = default;
    
public:
    static constexpr Node8 new_with(index_t x) {
        Node8 node{};
        node.insert(x);
        return node;
    }

    // Create a Node8 with all bits set except `x`.
    static constexpr Node8 new_all_but(index_t x) {
        Node8 node = new_with(x);
        node.not_inplace();
        return node;
    }

    // Create a Node8 with all bits set.
    static constexpr Node8 new_all() {
        Node8 node{};
        node.not_inplace();
        return node;
    }

    static constexpr std::size_t universe_size() {
        return 1uz + std::numeric_limits<index_t>::max();
    }

    constexpr Iterator begin() const {
        return min();
    }

    constexpr Iterator end() const {
        return Iterator::sentinel(this);
    }

    constexpr Iterator min() const {
        for (subindex_t word{}; word < num_words; ++word) {
            if (bits[word] != 0) {
                auto bit_idx{std::countr_zero(bits[word])};
                const auto value{index(word, static_cast<subindex_t>(bit_idx))};
                return Iterator::create(this, value);
            }
        }
        std::unreachable();
    }

    constexpr Iterator max() const {
        for (subindex_t word{num_words}; word > 0; --word) {
            if (bits[word - 1] != 0) {
                auto bit_idx{bits_per_word - 1 - std::countl_zero(bits[word - 1])};
                const auto value{index(word - 1, static_cast<subindex_t>(bit_idx))};
                return Iterator::create(this, value);
            }
        }
        std::unreachable();
    }

    constexpr void insert(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};
        bits[word_idx] |= (1ULL << bit_idx);
    }

    constexpr bool remove(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};

        if (!(bits[word_idx] & (1ULL << bit_idx))) {
            return false;
        }
        bits[word_idx] &= ~(1ULL << bit_idx);

        return empty();
    }

    constexpr bool contains(index_t x) const {
        const auto [word_idx, bit_idx] {decompose(x)};
        return (bits[word_idx] & (1ULL << bit_idx)) != 0;
    }

    constexpr Iterator successor(index_t x) const {
        const auto [start_word, start_bit] {decompose(x)};
        const std::uint64_t mask{~0ULL << (start_bit + 1)};

        std::uint64_t word{};
        if (start_bit + 1 < bits_per_word) {
            word = bits[start_word] & mask;
        }
        if (word != 0) {
            auto bit_idx{std::countr_zero(word)};
            const auto value{index(start_word, static_cast<subindex_t>(bit_idx))};
            return Iterator::create(this, value);
        }

        for (auto word_idx{start_word + 1}; word_idx < num_words; ++word_idx) {
            if (bits[word_idx] != 0) {
                auto bit_idx{std::countr_zero(bits[word_idx])};
                const auto value{index(static_cast<subindex_t>(word_idx), static_cast<subindex_t>(bit_idx))};
                return Iterator::create(this, value);
            }
        }

        return end();
    }

    constexpr Iterator predecessor(index_t x) const {
        if (x == 0) {
            return end();
        }

        const auto [start_word, start_bit] {decompose(x - 1)};
        const auto mask{start_bit == bits_per_word - 1 ? -1ULL : ((1ULL << (start_bit + 1)) - 1)};

        if (std::uint64_t word{bits[start_word] & mask}; word != 0) {
            auto bit_idx{bits_per_word - 1 - std::countl_zero(word)};
            const auto value{index(start_word, static_cast<subindex_t>(bit_idx))};
            return Iterator::create(this, value);
        }

        for (subindex_t word_idx{start_word}; word_idx > 0; --word_idx) {
            if (bits[word_idx - 1] != 0) {
                auto bit_idx{bits_per_word - 1 - std::countl_zero(bits[word_idx - 1])};
                const auto value{index(word_idx - 1, static_cast<subindex_t>(bit_idx))};
                return Iterator::create(this, value);
            }
        }

        return end();
    }

    constexpr std::size_t size() const {
        return std::popcount(bits[0]) + std::popcount(bits[1]) +
               std::popcount(bits[2]) + std::popcount(bits[3]);
    }

    constexpr bool full() const {
        return xsimd::all(load() == ~0ULL);
    }

    void serialize(std::string &out) const {
        for (std::uint64_t w : bits) {
            write_u64(out, w);
        }
    }

    static Node8 deserialize(std::string_view buf, std::size_t &pos) {
        Node8 node{};
        for (std::size_t i = 0; i < num_words; ++i) {
            node.bits[i] = read_u64(buf, pos);
        }
        return node;
    }

    struct count_range_args {
        index_t lo{0};
        index_t hi{static_cast<index_t>(universe_size() - 1)};
    };
    constexpr std::size_t count_range(count_range_args args) const {
        const auto [lo, hi] {args};
        const auto [lw, lb] {decompose(lo)};
        const auto [hw, hb] {decompose(hi)};
        const auto lmask{~0ULL << lb};
        const auto hmask{~0ULL >> (bits_per_word - 1 - hb)};

        if (lw == hw) {
            return std::popcount(bits[lw] & lmask & hmask);
        }

        auto acc{std::popcount(bits[lw] & lmask) +
                 std::popcount(bits[hw] & hmask)};
        for (int i = lw + 1; i < hw; ++i) {
            acc += std::popcount(bits[i]);
        }
        return acc;
    }

    static constexpr VebTreeMemoryStats get_memory_stats() {
        return {0, 0, 1};
    }

    // Truth table for set operations:
    //
    //  A | B | -A | A U B | A & B | A ^ B | A - B 
    // --------------------------------------------
    //  0 | 0 |  1 |   0   |   0   |   0   |   0
    //  0 | 1 |  1 |   1   |   0   |   1   |   0
    //  1 | 0 |  0 |   1   |   0   |   1   |   1
    //  1 | 1 |  0 |   1   |   1   |   0   |   0

    constexpr bool not_inplace() {
        auto v{load()};
        store(~v);
        return empty();
    }

    constexpr bool or_inplace(const Node8& other) {
        auto v1{load()};
        auto v2{other.load()};
        store(v1 | v2);
        // no need to check for emptiness, or can only ever grow a set
        return false;
    }

    constexpr bool xor_inplace(const Node8& other) {
        auto v1{load()};
        auto v2{other.load()};
        store(v1 ^ v2);
        return empty();
    }

    constexpr bool and_inplace(const Node8& other) {
        auto v1{load()};
        auto v2{other.load()};
        store(v1 & v2);
        return empty();
    }

    constexpr bool andnot_inplace(const Node8& other) {
        auto v1{load()};
        auto v2{other.load()};
        store(xsimd::bitwise_andnot(v2, v1));
        return empty();
    }
};

#endif // NODE8_HPP
