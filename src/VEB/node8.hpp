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


#if defined(__AVX2__)
    #include "immintrin.h"
    #define NODE8_AVX2_ENABLED
    #define NODE8_ALIGNMENT alignas(32)
    #define NODE8_CONSTEXPR inline
    #if defined(__BMI2__)
        #define NODE8_BMI2_ENABLED
    #endif
#else
    #define NODE8_ALIGNMENT
    #define NODE8_CONSTEXPR constexpr inline
#endif

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
    static constexpr int bits_per_word{std::numeric_limits<std::uint64_t>::digits};
    static constexpr int num_words{256 / bits_per_word};

    NODE8_ALIGNMENT std::array<std::uint64_t, num_words> bits_{};

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {x / bits_per_word, x % bits_per_word};
    }
    static constexpr index_t index(subindex_t word, subindex_t bit) {
        return static_cast<index_t>(word * bits_per_word + bit);
    }

#ifdef NODE8_AVX2_ENABLED
    NODE8_CONSTEXPR __m256i load() const {
        return _mm256_load_si256(reinterpret_cast<const __m256i*>(bits_.data()));
    }
    NODE8_CONSTEXPR void store(__m256i v) {
        _mm256_store_si256(reinterpret_cast<__m256i*>(bits_.data()), v);
    }
#endif

    NODE8_CONSTEXPR bool empty() const {
#ifdef NODE8_AVX2_ENABLED
        const __m256i v = load();
        return _mm256_testz_si256(v, v);
#else
        return std::ranges::all_of(bits_, [](std::uint64_t word) { return word == 0; });
#endif
    }
    
public:
    NODE8_CONSTEXPR index_t get_cluster_index(index_t key) const {
        const auto [target_word, target_bit] {decompose(key)};
        const std::uint64_t mask{(1ULL << target_bit) - 1};

        index_t count{};
        for (subindex_t word{}; word < target_word; ++word) {
            count += static_cast<index_t>(std::popcount(bits_[word]));
        }
        count += static_cast<index_t>(std::popcount(bits_[target_word] & mask));
        return count;
    }

    NODE8_CONSTEXPR explicit Node8(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    static NODE8_CONSTEXPR std::size_t universe_size() {
        return std::numeric_limits<index_t>::max();
    }

    NODE8_CONSTEXPR index_t min() const {
        for (subindex_t word{}; word < num_words; ++word) {
            if (bits_[word] != 0) {
                auto bit_idx{std::countr_zero(bits_[word])};
                return index(word, static_cast<subindex_t>(bit_idx));
            }
        }
        std::unreachable();
    }

    NODE8_CONSTEXPR index_t max() const {
        for (subindex_t word{num_words}; word > 0; --word) {
            if (bits_[word - 1] != 0) {
                auto bit_idx{bits_per_word - 1 - std::countl_zero(bits_[word - 1])};
                return index(word - 1, static_cast<subindex_t>(bit_idx));
            }
        }
        std::unreachable();
    }

    NODE8_CONSTEXPR void insert(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    NODE8_CONSTEXPR bool remove(index_t x) {
        const auto [word_idx, bit_idx] {decompose(x)};

        if (!(bits_[word_idx] & (1ULL << bit_idx))) {
            return false;
        }
        bits_[word_idx] &= ~(1ULL << bit_idx);

        return empty();
    }

    NODE8_CONSTEXPR bool contains(index_t x) const {
        const auto [word_idx, bit_idx] {decompose(x)};
        return (bits_[word_idx] & (1ULL << bit_idx)) != 0;
    }

    NODE8_CONSTEXPR std::optional<index_t> successor(index_t x) const {
#if defined(NODE8_BMI2_ENABLED)
        const auto [w, b] {decompose(x)};

        // Check current word
        if (const auto word = bits_[w] ^ _bzhi_u64(bits_[w], b + 1); word != 0) {
            return index(w, static_cast<subindex_t>(std::countr_zero(word)));
        }

        // Check subsequent words
        if (w < num_words - 1) {
            const auto v{load()};
            const auto zeros{_mm256_cmpeq_epi64(v, _mm256_setzero_si256())};
            const auto mask{(~_mm256_movemask_pd(_mm256_castsi256_pd(zeros)) & 0xF) >> (w + 1)};
            if (const auto v{static_cast<std::uint64_t>(mask)}; v != 0) {
                const auto next{static_cast<subindex_t>(std::countr_zero(v) + w + 1)};
                return index(next, static_cast<subindex_t>(std::countr_zero(bits_[next])));
            }
        }
#else
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
#endif
        return std::nullopt;
    }

    NODE8_CONSTEXPR std::optional<index_t> predecessor(index_t x) const {
#if defined(NODE8_BMI2_ENABLED)
        const auto [w, b] {decompose(x)};

        // Check current word
        if (const auto word = _bzhi_u64(bits_[w], b); word != 0) {
            return index(w, static_cast<subindex_t>(bits_per_word - 1 - std::countl_zero(word)));
        }

        // Check preceding words
        if (w > 0) {
            const auto v{load()};
            const auto zeros{_mm256_cmpeq_epi64(v, _mm256_setzero_si256())};
            const auto mask{(~_mm256_movemask_pd(_mm256_castsi256_pd(zeros)) & 0xF) & ((1 << w) - 1)};
            if (const auto v{static_cast<std::uint64_t>(mask)}; v != 0) {
                const auto prev{static_cast<subindex_t>(bits_per_word - 1 - std::countl_zero(v))};
                return index(prev, static_cast<subindex_t>(bits_per_word - 1 - std::countl_zero(bits_[prev])));
            }
        }
#else
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
#endif
        return std::nullopt;
    }

    NODE8_CONSTEXPR std::size_t size() const {
        return std::transform_reduce(
#if __cpp_lib_execution
            std::execution::unseq,
#endif
            bits_.cbegin(), bits_.cend(), 0uz, std::plus<>{},
            [](std::uint64_t word) { return std::popcount(word); }
        );
    }

    NODE8_CONSTEXPR VebTreeMemoryStats get_memory_stats() const {
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

    NODE8_CONSTEXPR bool not_inplace() {
#ifdef NODE8_AVX2_ENABLED
        const __m256i v{load()};
        const __m256i ones{_mm256_set1_epi64x(-1LL)};
        const __m256i not_v{_mm256_xor_si256(v, ones)};
        store(not_v);
#else
        for (std::size_t i{}; i < num_words; ++i) {
            bits_[i] = ~bits_[i];
        }
#endif
        return empty();
    }
    NODE8_CONSTEXPR bool or_inplace(const Node8& other) {
#ifdef NODE8_AVX2_ENABLED
        const __m256i v1{load()};
        const __m256i v2{other.load()};
        const __m256i or_v{_mm256_or_si256(v1, v2)};
        store(or_v);
#else
        for (std::size_t i{}; i < num_words; ++i) {
            bits_[i] |= other.bits_[i];
        }
#endif
        // no need to check for emptiness, oring can only ever grow a set
        return false;
    }
    NODE8_CONSTEXPR bool xor_inplace(const Node8& other) {
#ifdef NODE8_AVX2_ENABLED
        const __m256i v1{load()};
        const __m256i v2{other.load()};
        const __m256i xor_v{_mm256_xor_si256(v1, v2)};
        store(xor_v);
#else
        for (std::size_t i{}; i < num_words; ++i) {
            bits_[i] ^= other.bits_[i];
        }
#endif
        return empty();
    }
    NODE8_CONSTEXPR bool and_inplace(const Node8& other) {
#ifdef NODE8_AVX2_ENABLED
        const __m256i v1{load()};
        const __m256i v2{other.load()};
        const __m256i and_v{_mm256_and_si256(v1, v2)};
        store(and_v);
#else
        for (std::size_t i{}; i < num_words; ++i) {
            bits_[i] &= other.bits_[i];
        }
#endif
        return empty();
    }
    // difference: A \ B
    NODE8_CONSTEXPR bool and_not_inplace(const Node8& other) {
#ifdef NODE8_AVX2_ENABLED
        const __m256i v1{load()};
        const __m256i v2{other.load()};
        const __m256i andnot_v{_mm256_andnot_si256(v2, v1)};
        store(andnot_v);
#else
        for (std::size_t i{}; i < num_words; ++i) {
            bits_[i] &= ~other.bits_[i];
        }
#endif
        return empty();
    }
};

#endif // NODE8_HPP
