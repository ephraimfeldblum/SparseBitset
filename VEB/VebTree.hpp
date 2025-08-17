/**
 * @brief van Emde Boas Tree implementation for sparse bitsets
 * reference: https://en.wikipedia.org/wiki/Van_Emde_Boas_tree
 *
 * This implementation uses a recursive node cluster structure to store the elements.
 * Each node is associated with a summary structure to indicate which clusters contain elements.
 *
 * The tree can be visualized as shown below:
 * Node<log U = 32> 
 * ┌───────────────────────────────┐
 * | min, max: u32                 | ← Lazily propagated. Not inserted into clusters.
 * | cluster_data: * {             | ← Lazily constructed only if non-empty.
 * |   summary : Node<16>          | ← Tracks which clusters are non-empty.
 * |   clusters: HashSet<Node<16>> | ← Up to √U clusters, each of size √U. Use HashSet to exploit cache locality.
 * | }           |                 |
 * └─────────────|─────────────────┘
 * Node<16>      ▼
 * ┌───────────────────────────────┐
 * | key: u16                      | ← Store which cluster this node belongs to directly in padding bytes.
 * | min, max: u16                 |
 * | capacity: u16                 |
 * | cluster_data: * {             |
 * |   summary : Node<8>           | ← Used to index into clusters in constant time. Requires sorted clusters.
 * |   clusters: Array<Node<8>, …> | ← Up to 256 elements. FAM is more cache-friendly than HashMap.
 * | }           |                 |
 * └─────────────|─────────────────┘
 * Node<8>       ▼
 * ┌───────────────────────────────┐
 * | bits: Array<u64, 4>           | ← 256 bits, SIMD-friendly.
 * └───────────────────────────────┘
 */

#ifndef VEBTREE_HPP
#define VEBTREE_HPP

#include <algorithm>
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <ranges>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <execution>

// #if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64)
// #include <immintrin.h>
// #endif

#include "allocator/tracking_allocator.hpp"
#include "allocator/allocate_unique.hpp"

struct VebTreeMemoryStats {
    std::size_t total_clusters = 0;
    std::size_t max_depth = 0;
    std::size_t total_nodes = 0;
};

template<typename S>
using index_t = typename std::remove_cvref_t<S>::index_t;

template<typename... Fs>
struct overload : Fs... {
    using Fs::operator()...;
};

template<typename... Fs>
overload(Fs...) -> overload<Fs...>;

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

        index_t count = 0;

        for (subindex_t word = 0; word < target_word; ++word) {
            count += static_cast<index_t>(std::popcount(bits_[word]));
        }

        std::uint64_t mask = (1ULL << target_bit) - 1;
        count += static_cast<index_t>(std::popcount(bits_[target_word] & mask));

        return count;
    }

    constexpr inline explicit Node8(index_t x, [[maybe_unused]] std::size_t& alloc) {
        const auto [word_idx, bit_idx] = decompose(x);
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    static constexpr std::size_t universe_size() { return UINT8_MAX; }

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

    constexpr inline void insert(index_t x, [[maybe_unused]] std::size_t& alloc) {
        const auto [word_idx, bit_idx] = decompose(x);
        bits_[word_idx] |= (1ULL << bit_idx);
    }

    constexpr inline bool remove(index_t x, [[maybe_unused]] std::size_t& alloc) {
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

        std::uint64_t word = 0;
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
// #if defined(__AVX2__) && (defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) || defined(__amd64))
//         static const __m256i lookup = _mm256_setr_epi8(
//             0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4,
//             0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4
//         );

//         __m256i vec = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(bits_.data()));

//         __m256i low  = _mm256_and_si256(vec, _mm256_set1_epi8(0x0F));
//         __m256i high = _mm256_and_si256(_mm256_srli_epi16(vec, 4), _mm256_set1_epi8(0x0F));

//         __m256i popcnt1 = _mm256_shuffle_epi8(lookup, low);
//         __m256i popcnt2 = _mm256_shuffle_epi8(lookup, high);

//         __m256i popcnt = _mm256_add_epi8(popcnt1, popcnt2);
//         __m256i zero = _mm256_setzero_si256();
//         __m256i sad = _mm256_sad_epu8(popcnt, zero);

//         alignas(32) std::uint64_t partial[4];
//         _mm256_store_si256(reinterpret_cast<__m256i*>(partial), sad);

//         return partial[0] + partial[1] + partial[2] + partial[3];
// #endif
        return std::popcount(bits_[0]) + std::popcount(bits_[1]) +
               std::popcount(bits_[2]) + std::popcount(bits_[3]);
    }

    constexpr VebTreeMemoryStats get_memory_stats() const {
        return {0, 0, 1};
    }

    Node8 clone([[maybe_unused]] std::size_t& alloc) const {
        return *this;
    }

    Node8& or_inplace(const Node8& other, [[maybe_unused]] std::size_t& alloc) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] |= other.bits_[i];
        }
        return *this;
    }
    bool is_tombstone() const {
        return size() == 0;
    }

    Node8& and_inplace(const Node8& other, [[maybe_unused]] std::size_t& alloc) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] &= other.bits_[i];
        }
        return *this;
    }
    Node8& xor_inplace(const Node8& other, [[maybe_unused]] std::size_t& alloc) {
        for (std::size_t i = 0; i < 4; ++i) {
            bits_[i] ^= other.bits_[i];
        }
        return *this;
    }
};

class Node16 {
    friend class VebTree;
public:
    using subnode_t = Node8;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint16_t;

private:
    struct cluster_data_t {
        subnode_t summary_;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        subnode_t clusters_[];
#pragma GCC diagnostic pop

        subindex_t index_of(subindex_t x) const {
            return summary_.get_cluster_index(x);
        }
        subnode_t* find(subindex_t x) {
            return summary_.contains(x) ? &clusters_[index_of(x)] : nullptr;
        }
        const subnode_t* find(subindex_t x) const {
            return summary_.contains(x) ? &clusters_[index_of(x)] : nullptr;
        }
        std::size_t size() const {
            return summary_.size();
        }
    };
    cluster_data_t* cluster_data_ = nullptr;
    tracking_allocator<subnode_t> alloc_;
    std::uint16_t capacity_ = 0;
    index_t key_;
    index_t min_;
    index_t max_;

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 8), static_cast<subindex_t>(x)};
    }
    static constexpr index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 8 | low;
    }

    subnode_t* find(subindex_t x) {
        return cluster_data_ ? cluster_data_->find(x) : nullptr;
    }
    const subnode_t* find(subindex_t x) const {
        return cluster_data_ ? cluster_data_->find(x) : nullptr;
    }

    inline void grow_capacity(std::uint16_t new_capacity) {
        auto* ptr = alloc_.allocate(new_capacity + 1);
        auto* new_data = reinterpret_cast<cluster_data_t*>(ptr);
        new_data->summary_ = cluster_data_->summary_;
        std::copy(cluster_data_->clusters_, cluster_data_->clusters_ + cluster_data_->size(), new_data->clusters_);
        ptr = reinterpret_cast<subnode_t*>(cluster_data_);
        alloc_.deallocate(ptr, capacity_ + 1);
        cluster_data_ = new_data;
        capacity_ = new_capacity;
    }

    inline void emplace(subindex_t hi, subindex_t lo, std::size_t& alloc) {
        if (!cluster_data_) {
            auto ptr = tracking_allocator<subnode_t>(alloc).allocate(2);
            cluster_data_ = reinterpret_cast<cluster_data_t*>(ptr);
            cluster_data_->summary_ = subnode_t(hi, alloc);
            cluster_data_->clusters_[0] = subnode_t(lo, alloc);
            capacity_ = 1;
            return;
        }

        const std::uint8_t idx = cluster_data_->index_of(hi);
        if (cluster_data_->summary_.contains(hi)) {
            cluster_data_->clusters_[idx].insert(lo, alloc);
            return;
        }

        const std::size_t size = cluster_data_->size();
        if (size == capacity_) {
            grow_capacity(static_cast<std::uint16_t>(std::min(256, capacity_ + (capacity_ >> 2) + 1)));
        }
        if (idx < size) {
            std::copy(cluster_data_->clusters_ + idx, cluster_data_->clusters_ + size, cluster_data_->clusters_ + idx + 1);
        }
        cluster_data_->clusters_[idx] = subnode_t(lo, alloc);
        cluster_data_->summary_.insert(hi, alloc);
    }

public:
    inline explicit Node16(index_t hi, index_t lo, std::size_t& alloc)
        : cluster_data_(nullptr), alloc_(alloc), capacity_(0), key_(hi), min_(lo), max_(lo) {
    }

    inline Node16(Node8&& old_storage, std::size_t& alloc)
        : cluster_data_(nullptr)
        , alloc_(alloc)
        , capacity_(0)
        , key_(0)
        , min_(old_storage.min())
        , max_(old_storage.max())
    {
        auto old_min = static_cast<subindex_t>(min_);
        auto old_max = static_cast<subindex_t>(max_);

        old_storage.remove(old_min, alloc);
        if (old_min != old_max) {
            old_storage.remove(old_max, alloc);
        }

        if (old_storage.size() > 0) {
            auto ptr = tracking_allocator<Node8>(alloc).allocate(2);
            cluster_data_ = reinterpret_cast<cluster_data_t*>(ptr);
            cluster_data_->summary_ = Node8(0, alloc);
            capacity_ = 1;
            cluster_data_->clusters_[0] = std::move(old_storage);
        }
    }

    Node16 clone(std::size_t& alloc) const {
        Node16 result(key_, min_, alloc);
        result.min_ = min_;
        result.max_ = max_;

        if (cluster_data_) {
            const std::size_t size = cluster_data_->size();
            auto ptr = tracking_allocator<subnode_t>(alloc).allocate(size + 1);
            result.cluster_data_ = reinterpret_cast<cluster_data_t*>(ptr);
            result.capacity_ = static_cast<std::uint16_t>(size);
            result.cluster_data_->summary_ = cluster_data_->summary_;
            std::copy(cluster_data_->clusters_, cluster_data_->clusters_ + size, result.cluster_data_->clusters_);
        }
        return result;
    }

    Node16(Node16&& other) noexcept
        : cluster_data_(std::exchange(other.cluster_data_, nullptr))
        , alloc_(other.alloc_)
        , capacity_(std::exchange(other.capacity_, 0))
        , key_(other.key_)
        , min_(other.min_)
        , max_(other.max_) {
    }

    Node16& operator=(Node16&& other) noexcept {
        if (this != &other) {
            this->~Node16();
            new (this) Node16(std::move(other));
        }
        return *this;
    }

    ~Node16() noexcept {
        if (cluster_data_) {
            alloc_.deallocate(reinterpret_cast<subnode_t*>(cluster_data_), capacity_ + 1);
        }
    }

    static constexpr std::size_t universe_size() { return UINT16_MAX; }
    constexpr index_t min() const { return min_; }
    constexpr index_t max() const { return max_; }

    inline void insert(index_t x, std::size_t& alloc) {
        if (x < min_) {
            std::swap(x, min_);
        }
        if (x > max_) {
            std::swap(x, max_);
        }
        if (x == min_ || x == max_) {
            return;
        }

        const auto [h, l] = decompose(x);
        emplace(h, l, alloc);
    }

    inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (!cluster_data_ || cluster_data_->size() == 0) {
                if (max_ == min_) {
                    return true;
                } else {
                    min_ = max_;
                    return false;
                }
            } else {
                auto min_cluster = cluster_data_->summary_.min();
                auto min_element = cluster_data_->clusters_[cluster_data_->index_of(min_cluster)].min();
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (!cluster_data_ || cluster_data_->size() == 0) {
                if (max_ == min_) {
                    return true;
                } else {
                    max_ = min_;
                    return false;
                }
            } else {
                auto max_cluster = cluster_data_->summary_.max();
                auto max_element = cluster_data_->clusters_[cluster_data_->index_of(max_cluster)].max();
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] = decompose(x);

        if (auto* cluster = find(h)) {
            if (cluster->remove(l, alloc)) {
                const std::uint8_t idx = cluster_data_->index_of(h);
                const std::size_t size = cluster_data_->size();

                std::copy(cluster_data_->clusters_ + idx + 1, cluster_data_->clusters_ + size, cluster_data_->clusters_ + idx);

                cluster_data_->summary_.remove(h, alloc);

                if (cluster_data_->size() == 0) {
                    alloc_.deallocate(reinterpret_cast<subnode_t*>(cluster_data_), capacity_ + 1);
                    cluster_data_ = nullptr;
                    capacity_ = 0;
                }
            }
        }

        return false;
    }

    inline bool contains(index_t x) const {
        if (x == min_ || x == max_) {
            return true;
        }

        const auto [h, l] = decompose(x);
        if (const auto* cluster = find(h)) {
            return cluster->contains(l);
        }
        return false;
    }

    inline std::optional<index_t> successor(index_t x) const {
        if (x < min_) {
            return min_;
        }
        if (x >= max_) {
            return std::nullopt;
        }

        if (!cluster_data_) {
            return std::make_optional(max_);
        }

        const auto [h, l] = decompose(x);

        if (const auto* cluster = find(h)) {
            if (l < cluster->max()) {
                if (auto succ = cluster->successor(l)) {
                    return index(h, *succ);
                }
            }
        }

        if (auto succ_cluster = cluster_data_->summary_.successor(h)) {
            auto min_element = cluster_data_->clusters_[cluster_data_->index_of(*succ_cluster)].min();
            return index(*succ_cluster, min_element);
        }

        return std::make_optional(max_);
    }

    inline std::optional<index_t> predecessor(index_t x) const {
        if (x > max_) {
            return max_;
        }
        if (x <= min_) {
            return std::nullopt;
        }

        if (!cluster_data_) return min_;

        const auto [h, l] = decompose(x);

        if (const auto* cluster = find(h)) {
            if (l > cluster->min()) {
                if (auto pred = cluster->predecessor(l)) {
                    return index(h, *pred);
                }
            }
        }

        if (auto pred_cluster = cluster_data_->summary_.predecessor(h)) {
            auto max_element = cluster_data_->clusters_[cluster_data_->index_of(*pred_cluster)].max();
            return index(*pred_cluster, max_element);
        }

        return min_;
    }

    inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (!cluster_data_) return base_count;

        const auto* data = &cluster_data_->clusters_[0];
        std::size_t count = cluster_data_->size();
        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            data, data + count, base_count, std::plus<>(),
            [] (const auto& node) { return node.size(); }
        );
    }

    constexpr inline VebTreeMemoryStats get_memory_stats() const {
        if (!cluster_data_) {
            return {0, 0, 1};
        }

        auto stats = cluster_data_->summary_.get_memory_stats();
        stats.total_nodes += 1;
        stats.max_depth += 1;
        stats.total_clusters += cluster_data_->size();

        const std::size_t cluster_count = cluster_data_->size();
        for (std::size_t i = 0; i < cluster_count; ++i) {
            auto cluster_stats = cluster_data_->clusters_[i].get_memory_stats();
            stats.total_nodes += cluster_stats.total_nodes;
            stats.max_depth = std::max(stats.max_depth, cluster_stats.max_depth + 1);
        }

        return stats;
    }

    Node16& or_inplace(const Node16& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (!other.cluster_data_) {
            return *this;
        }

        if (!cluster_data_) {
            const std::size_t size = other.cluster_data_->size();
            auto ptr = tracking_allocator<subnode_t>(alloc).allocate(size + 1);
            cluster_data_ = reinterpret_cast<cluster_data_t*>(ptr);
            capacity_ = static_cast<std::uint16_t>(size);
            cluster_data_->summary_ = other.cluster_data_->summary_.clone(alloc);
            std::copy(other.cluster_data_->clusters_, other.cluster_data_->clusters_ + size, cluster_data_->clusters_);

            return *this;
        }

        if (auto merge_summary = cluster_data_->summary_.clone(alloc).or_inplace(other.cluster_data_->summary_, alloc); merge_summary.size() != cluster_data_->size()) {
            auto ptr = tracking_allocator<subnode_t>(alloc).allocate(merge_summary.size() + 1);
            auto new_cluster_data = reinterpret_cast<cluster_data_t*>(ptr);
            auto new_capacity = static_cast<std::uint16_t>(merge_summary.size());
            new_cluster_data->summary_ = std::move(merge_summary);

            std::size_t i = 0;
            std::size_t j = 0;
            std::size_t k = 0;
            for (auto idx = std::make_optional(new_cluster_data->summary_.min()); idx.has_value(); idx = new_cluster_data->summary_.successor(*idx)) {
                const bool in_this = cluster_data_->summary_.contains(*idx);
                const bool in_other = other.cluster_data_->summary_.contains(*idx);
                if (in_this && in_other) {
                    new_cluster_data->clusters_[k++] = cluster_data_->clusters_[i++].or_inplace(other.cluster_data_->clusters_[j++], alloc); 
                } else if (in_this) {
                    new_cluster_data->clusters_[k++] = cluster_data_->clusters_[i++];
                } else if (in_other) {
                    new_cluster_data->clusters_[k++] = other.cluster_data_->clusters_[j++].clone(alloc);
                } else {
                    std::unreachable();
                }
            }
            alloc_.deallocate(reinterpret_cast<subnode_t*>(cluster_data_), capacity_ + 1);
            cluster_data_ = new_cluster_data;
            capacity_ = new_capacity;
            return *this;
        }

        std::size_t i = 0;
        std::size_t j = 0;
        for (auto idx = std::make_optional(cluster_data_->summary_.min()); idx.has_value(); idx = cluster_data_->summary_.successor(*idx)) {
            if (other.cluster_data_->summary_.contains(*idx)) {
                cluster_data_->clusters_[i].or_inplace(other.cluster_data_->clusters_[j++], alloc);
            }
            ++i;
        }
        return *this;
    }

    Node16& empty_clusters_or_tombstone(std::optional<index_t> new_min, std::optional<index_t> new_max) {
        alloc_.deallocate(reinterpret_cast<subnode_t*>(cluster_data_), capacity_ + 1);
        cluster_data_ = nullptr;
        capacity_ = 0;
        if (new_min.has_value() && new_max.has_value()) {
            min_ = *new_min;
            max_ = *new_max;
        } else if (new_min.has_value()) {
            min_ = *new_min;
            max_ = *new_min;
        } else if (new_max.has_value()) {
            min_ = *new_max;
            max_ = *new_max;
        } else {
            min_ = std::numeric_limits<index_t>::max();
            max_ = std::numeric_limits<index_t>::min();
        }
        return *this;
    }

    bool is_tombstone() const {
        return min_ > max_;
    }

    Node16& and_inplace(const Node16& other, std::size_t& alloc) {
        index_t potential_min = std::max(min_, other.min_);
        index_t potential_max = std::min(max_, other.max_);
        auto new_min = contains(potential_min) && other.contains(potential_min) ? std::make_optional(potential_min) : std::nullopt;
        auto new_max = contains(potential_max) && other.contains(potential_max) ? std::make_optional(potential_max) : std::nullopt;
        if (potential_min >= potential_max || !cluster_data_ || !other.cluster_data_) {
            return empty_clusters_or_tombstone(new_min, new_max);
        }
        subnode_t summary_intersection = cluster_data_->summary_.clone(alloc).and_inplace(other.cluster_data_->summary_, alloc);
        if (summary_intersection.is_tombstone()) {
            return empty_clusters_or_tombstone(new_min, new_max);
        }

        std::size_t write_idx = 0;
        for (auto cluster_idx = std::make_optional(summary_intersection.min()); cluster_idx.has_value(); cluster_idx = summary_intersection.successor(*cluster_idx)) {
            const subindex_t this_cluster_pos = cluster_data_->index_of(*cluster_idx);
            const subindex_t other_cluster_pos = other.cluster_data_->index_of(*cluster_idx);
            auto& this_cluster = cluster_data_->clusters_[this_cluster_pos];
            auto& other_cluster = other.cluster_data_->clusters_[other_cluster_pos];

            if (!this_cluster.and_inplace(other_cluster, alloc).is_tombstone()) {
                if (write_idx != this_cluster_pos) {
                    cluster_data_->clusters_[write_idx] = this_cluster;
                }
                write_idx++;
            } else if (summary_intersection.remove(*cluster_idx, alloc)) {
                return empty_clusters_or_tombstone(new_min, new_max);
            }
        }
        cluster_data_->summary_ = summary_intersection;

        min_ = *new_min.or_else([&] {
            auto min_cluster = cluster_data_->summary_.min();
            auto min_element = cluster_data_->clusters_[0].min();
            return std::make_optional(index(min_cluster, min_element));
        });
        max_ = *new_max.or_else([&] {
            auto max_cluster = cluster_data_->summary_.max();
            auto max_element = cluster_data_->clusters_[cluster_data_->size() - 1].max();
            return std::make_optional(index(max_cluster, max_element));
        });
        if (max_ != potential_max && cluster_data_->clusters_[cluster_data_->size() - 1].remove(static_cast<subindex_t>(max_), alloc)) {
            cluster_data_->summary_.remove(cluster_data_->summary_.max(), alloc);
        }
        if (min_ != potential_min && cluster_data_->clusters_[0].remove(static_cast<subindex_t>(min_), alloc)) {
            cluster_data_->summary_.remove(cluster_data_->summary_.min(), alloc);
            std::copy(cluster_data_->clusters_ + 1, cluster_data_->clusters_ + write_idx, cluster_data_->clusters_);
        }
        if (cluster_data_->size() == 0) {
            return empty_clusters_or_tombstone(min_, max_);
        }

        return *this;
    }

    friend struct std::hash<Node16>;
    friend struct std::equal_to<Node16>;
};

template<>
struct std::equal_to<Node16> {
    using is_transparent = void;
    bool operator()(const Node16& lhs, const Node16& rhs) const {
        return lhs.key_ == rhs.key_;
    }
    bool operator()(const Node16& lhs, const Node16::index_t& rhs) const {
        return lhs.key_ == rhs;
    }
    bool operator()(const Node16::index_t& lhs, const Node16& rhs) const {
        return lhs == rhs.key_;
    }
    bool operator()(const Node16::index_t& lhs, const Node16::index_t& rhs) const {
        return lhs == rhs;
    }
};
template<>
struct std::hash<Node16> {
    using is_transparent = void;
    std::size_t operator()(const Node16& node) const {
        return std::hash<Node16::index_t>()(node.key_);
    }
    std::size_t operator()(const Node16::index_t& key) const {
        return std::hash<Node16::index_t>()(key);
    }
};

class Node32 {
    friend class VebTree;
public:
    using subnode_t = Node16;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint32_t;

private:
    struct cluster_data_t {
        using cluster_map_t = std::unordered_set<subnode_t, std::hash<subnode_t>, std::equal_to<subnode_t>, tracking_allocator<subnode_t>>;
        cluster_map_t clusters;
        subnode_t summary;

        cluster_data_t(subindex_t x, std::size_t& alloc)
            : clusters(tracking_allocator<subnode_t>(alloc)),
              summary(0, x, alloc) {}
    };
    using clusters_t = std::unique_ptr<cluster_data_t, AllocDeleter<tracking_allocator<cluster_data_t>>>;
    clusters_t cluster_data_;
    
    index_t min_;
    index_t max_;

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 16), static_cast<subindex_t>(x)};
    }
    static constexpr index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 16 | low;
    }

public:
    inline explicit Node32(index_t x, std::size_t& alloc)
        : cluster_data_{nullptr, AllocDeleter{tracking_allocator<cluster_data_t>(alloc)}}
        , min_(x), max_(x) {
    }

    inline Node32(Node16&& old_storage, std::size_t& alloc)
        : cluster_data_{nullptr, AllocDeleter{tracking_allocator<cluster_data_t>(alloc)}}
        , min_(old_storage.min())
        , max_(old_storage.max())
    {
        auto old_min = static_cast<subindex_t>(min_);
        auto old_max = static_cast<subindex_t>(max_);

        old_storage.remove(old_min, alloc);
        if (old_min != old_max) {
            old_storage.remove(old_max, alloc);
        }

        if (old_storage.size() > 0) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, 0, alloc);
            cluster_data_->clusters.emplace(std::move(old_storage));
        }
    }

    Node32 clone(std::size_t& alloc) const {
        Node32 result(min_, alloc);
        result.min_ = min_;
        result.max_ = max_;

        if (cluster_data_) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            result.cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, 0, alloc);
            result.cluster_data_->summary = cluster_data_->summary.clone(alloc);
            for (const auto& cluster : cluster_data_->clusters) {
                result.cluster_data_->clusters.emplace(cluster.clone(alloc));
            }
        }
        return result;
    }

    Node32(Node32&& other) noexcept = default;
    Node32& operator=(Node32&& other) noexcept = default;

    static constexpr std::size_t universe_size() { return UINT32_MAX; }
    constexpr index_t min() const { return min_; }
    constexpr index_t max() const { return max_; }

    inline void insert(index_t x, std::size_t& alloc) {
        if (x < min_) {
            std::swap(x, min_);
        }
        if (x > max_) {
            std::swap(x, max_);
        }
        if (x == min_ || x == max_) {
            return;
        }

        const auto [h, l] = decompose(x);

        if (!cluster_data_) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, h, alloc);
            cluster_data_->clusters.emplace(h, l, alloc);
        } else if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end()) {
            auto& cluster = const_cast<Node16&>(*it);
            cluster.insert(l, alloc);
        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.emplace(h, l, alloc);
        }
    }

    inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (!cluster_data_ || cluster_data_->clusters.empty()) {
                return true;
            } else {
                auto min_cluster = cluster_data_->summary.min();
                auto min_element = cluster_data_->clusters.find(min_cluster)->min();
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (!cluster_data_ || cluster_data_->clusters.empty()) {
                max_ = min_;
            } else {
                auto max_cluster = cluster_data_->summary.max();
                auto max_element = cluster_data_->clusters.find(max_cluster)->max();
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] = decompose(x);

        if (cluster_data_) {
            if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end()) {
                auto& cluster = const_cast<Node16&>(*it);
                if (cluster.remove(l, alloc)) {
                    cluster_data_->clusters.erase(it);
                    cluster_data_->summary.remove(h, alloc);
                    if (cluster_data_->clusters.empty()) {
                        cluster_data_.reset();
                    }
                }
            }
        }



        return false;
    }

    inline bool contains(index_t x) const {
        if (x == min_ || x == max_) {
            return true;
        }

        if (!cluster_data_) return false;

        const auto [h, l] = decompose(x);
        if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end()) {
            return it->contains(l);
        }
        return false;
    }

    inline std::optional<index_t> successor(index_t x) const {
        if (x < min_) {
            return min_;
        }
        if (x >= max_) {
            return std::nullopt;
        }

        if (!cluster_data_) {
            return std::make_optional(max_);
        }

        const auto [h, l] = decompose(x);

        if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end() && l < it->max()) {
            if (auto succ = it->successor(l)) {
                return index(h, *succ);
            }
        }

        if (auto succ_cluster = cluster_data_->summary.successor(h)) {
            auto min_element = cluster_data_->clusters.find(*succ_cluster)->min();
            return index(*succ_cluster, min_element);
        }

        return std::make_optional(max_);
    }

    inline std::optional<index_t> predecessor(index_t x) const {
        if (x > max_) {
            return max_;
        }
        if (x <= min_) {
            return std::nullopt;
        }

        if (!cluster_data_) return min_;

        const auto [h, l] = decompose(x);

        if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end() && l > it->min()) {
            if (auto pred = it->predecessor(l)) {
                return index(h, *pred);
            }
        }

        if (auto pred_cluster = cluster_data_->summary.predecessor(h)) {
            auto max_element = cluster_data_->clusters.find(*pred_cluster)->max();
            return index(*pred_cluster, max_element);
        }

        return min_;
    }

    inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (!cluster_data_) return base_count;

        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            cluster_data_->clusters.begin(), cluster_data_->clusters.end(),
            base_count, std::plus<>(), [](const auto& cluster) { return cluster.size(); }
        );
    }

    inline VebTreeMemoryStats get_memory_stats() const {
        return cluster_data_ ? std::ranges::fold_left(
            cluster_data_->clusters,
            [&] {
                auto stats = cluster_data_->summary.get_memory_stats();
                stats.total_clusters += cluster_data_->clusters.size();
                stats.total_nodes += 1;
                return stats;
            }(),
            [](VebTreeMemoryStats acc, const auto& cluster) {
                auto cluster_stats = cluster.get_memory_stats();
                acc.total_nodes += cluster_stats.total_nodes;
                acc.total_clusters += cluster_stats.total_clusters;
                acc.max_depth = std::max(acc.max_depth, cluster_stats.max_depth + 1);
                return acc;
            }
        ) : VebTreeMemoryStats{0, 0, 1};
    }

    Node32& or_inplace(const Node32& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (!other.cluster_data_) {
            return *this;
        }

        if (!cluster_data_) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, 0, alloc);
            cluster_data_->summary = other.cluster_data_->summary.clone(alloc);
            for (const auto& cluster : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(cluster.clone(alloc));
            }

            return *this;
        }

        cluster_data_->summary.or_inplace(other.cluster_data_->summary, alloc);
        for (const auto& other_cluster : other.cluster_data_->clusters) {
            if (auto it = cluster_data_->clusters.find(other_cluster); it != cluster_data_->clusters.end()) {
                auto& cluster = const_cast<Node16&>(*it);
                cluster.or_inplace(other_cluster, alloc);
            } else {
                cluster_data_->clusters.emplace(other_cluster.clone(alloc));
            }
        }
        return *this;
    }

private:
    Node32& empty_clusters_or_tombstone(std::optional<index_t> new_min, std::optional<index_t> new_max) {
        cluster_data_.reset();
        if (new_min.has_value() && new_max.has_value()) {
            min_ = *new_min;
            max_ = *new_max;
        } else if (new_min.has_value()) {
            min_ = *new_min;
            max_ = *new_min;
        } else if (new_max.has_value()) {
            min_ = *new_max;
            max_ = *new_max;
        } else {
            min_ = std::numeric_limits<index_t>::max();
            max_ = std::numeric_limits<index_t>::min();
        }
        return *this;
    }

public:
    bool is_tombstone() const {
        return min_ > max_;
    }

    Node32& and_inplace(const Node32& other, std::size_t& alloc) {
        index_t potential_min = std::max(min_, other.min_);
        index_t potential_max = std::min(max_, other.max_);
        auto new_min = contains(potential_min) && other.contains(potential_min) ? std::make_optional(potential_min) : std::nullopt;
        auto new_max = contains(potential_max) && other.contains(potential_max) ? std::make_optional(potential_max) : std::nullopt;
        if (potential_min >= potential_max || !cluster_data_ || !other.cluster_data_) {
            return empty_clusters_or_tombstone(new_min, new_max);
        }

        if (cluster_data_->summary.and_inplace(other.cluster_data_->summary, alloc).is_tombstone()) {
            return empty_clusters_or_tombstone(new_min, new_max);
        }

        for (auto cluster_idx = std::make_optional(cluster_data_->summary.min()); cluster_idx.has_value(); cluster_idx = cluster_data_->summary.successor(*cluster_idx)) {
            if (auto this_it = cluster_data_->clusters.find(*cluster_idx), other_it = other.cluster_data_->clusters.find(*cluster_idx);
                this_it != cluster_data_->clusters.end() && other_it != other.cluster_data_->clusters.end()) {
                if (auto& cluster = const_cast<Node16&>(*this_it);
                    cluster.and_inplace(*other_it, alloc).is_tombstone() && (cluster_data_->clusters.erase(this_it), cluster_data_->summary.remove(*cluster_idx, alloc))) {
                    return empty_clusters_or_tombstone(new_min, new_max);
                }
            }
        }

        min_ = *new_min.or_else([&] {
            auto min_cluster = cluster_data_->summary.min();
            auto min_element = cluster_data_->clusters.find(min_cluster)->min();
            return std::make_optional(index(min_cluster, min_element));
        });
        max_ = *new_max.or_else([&] {
            auto max_cluster = cluster_data_->summary.max();
            auto max_element = cluster_data_->clusters.find(max_cluster)->max();
            return std::make_optional(index(max_cluster, max_element));
        });

        if (max_ != potential_max) {
            auto it = cluster_data_->clusters.find(cluster_data_->summary.max());
            auto& cluster = const_cast<Node16&>(*it);
            cluster.remove(static_cast<subindex_t>(max_), alloc) && cluster_data_->summary.remove(cluster_data_->summary.max(), alloc);
        }
        if (min_ != potential_min) {
            auto it = cluster_data_->clusters.find(cluster_data_->summary.min());
            auto& cluster = const_cast<Node16&>(*it);
            cluster.remove(static_cast<subindex_t>(min_), alloc) && cluster_data_->summary.remove(cluster_data_->summary.min(), alloc);
        }

        if (cluster_data_->clusters.empty()) {
            return empty_clusters_or_tombstone(min_, max_);
        }

        return *this;
    }
};

class Node64 {
    friend class VebTree;
public:
    using subnode_t = Node32;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint64_t;

private:
    struct cluster_data_t {
        using cluster_map_t = std::unordered_map<subindex_t, subnode_t, std::hash<subindex_t>, std::equal_to<subindex_t>, tracking_allocator<std::pair<const subindex_t, subnode_t>>>;
        cluster_map_t clusters;
        subnode_t summary;

        cluster_data_t(subindex_t x, std::size_t& alloc)
            : clusters(tracking_allocator<std::pair<const subindex_t, subnode_t>>(alloc)),
              summary(x, alloc) {}

        auto values() const { return std::views::values(clusters); }
        auto values() { return std::views::values(clusters); }
    };
    using clusters_t = std::unique_ptr<cluster_data_t, AllocDeleter<tracking_allocator<cluster_data_t>>>;
    clusters_t cluster_data_;
    index_t min_;
    index_t max_;

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 32), static_cast<subindex_t>(x)};
    }
    static constexpr index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 32 | low;
    }

public:
    inline explicit Node64(index_t x, std::size_t& alloc)
        : cluster_data_{nullptr, AllocDeleter{tracking_allocator<cluster_data_t>(alloc)}}
        , min_(x), max_(x) {
    }

    inline Node64(Node32&& old_storage, std::size_t& alloc)
        : cluster_data_{nullptr, AllocDeleter{tracking_allocator<cluster_data_t>(alloc)}}
        , min_(old_storage.min())
        , max_(old_storage.max())
    {
        auto old_min = static_cast<subindex_t>(min_);
        auto old_max = static_cast<subindex_t>(max_);

        old_storage.remove(old_min, alloc);
        if (old_min != old_max) {
            old_storage.remove(old_max, alloc);
        }

        if (old_storage.size() > 0) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, 0, alloc);
            cluster_data_->clusters.emplace(0, std::move(old_storage));
        }
    }

    static constexpr std::uint64_t universe_size() { return UINT64_MAX; }
    constexpr index_t min() const { return min_; }
    constexpr index_t max() const { return max_; }

    inline void insert(index_t x, std::size_t& alloc) {
        if (x < min_) {
            std::swap(x, min_);
        }
        if (x > max_) {
            std::swap(x, max_);
        }
        if (x == min_ || x == max_) {
            return;
        }

        const auto [h, l] = decompose(x);

        if (!cluster_data_) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, h, alloc);
            cluster_data_->clusters.try_emplace(h, l, alloc);
        } else if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end()) {
            it->second.insert(l, alloc);
        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.try_emplace(h, l, alloc);
        }
    }

    inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (!cluster_data_ || cluster_data_->clusters.empty()) {
                return true;
            } else {
                auto min_cluster = cluster_data_->summary.min();
                auto min_element = cluster_data_->clusters.at(min_cluster).min();
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (!cluster_data_ || cluster_data_->clusters.empty()) {
                max_ = min_;
            } else {
                auto max_cluster = cluster_data_->summary.max();
                auto max_element = cluster_data_->clusters.at(max_cluster).max();
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] = decompose(x);

        if (cluster_data_) {
            if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end()) {
                if (it->second.remove(l, alloc)) {
                    cluster_data_->clusters.erase(it);
                    cluster_data_->summary.remove(h, alloc);
                    if (cluster_data_->clusters.empty()) {
                        cluster_data_.reset();
                    }
                }
            }
        }



        return false;
    }

    inline bool contains(index_t x) const {
        if (x == min_ || x == max_) {
            return true;
        }

        if (!cluster_data_) return false;

        const auto [h, l] = decompose(x);
        if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end()) {
            return it->second.contains(l);
        }
        return false;
    }

    inline std::optional<index_t> successor(index_t x) const {
        if (x < min_) {
            return min_;
        }
        if (x >= max_) {
            return std::nullopt;
        }

        if (!cluster_data_) {
            return std::make_optional(max_);
        }

        const auto [h, l] = decompose(x);

        if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end() && l < it->second.max()) {
            if (auto succ = it->second.successor(l)) {
                return index(h, *succ);
            }
        }

        if (auto succ_cluster = cluster_data_->summary.successor(h)) {
            auto min_element = cluster_data_->clusters.at(*succ_cluster).min();
            return index(*succ_cluster, min_element);
        }

        return std::make_optional(max_);
    }

    inline std::optional<index_t> predecessor(index_t x) const {
        if (x > max_) {
            return max_;
        }
        if (x <= min_) {
            return std::nullopt;
        }

        if (!cluster_data_) return min_;

        const auto [h, l] = decompose(x);

        if (auto it = cluster_data_->clusters.find(h); it != cluster_data_->clusters.end() && l > it->second.min()) {
            if (auto pred = it->second.predecessor(l)) {
                return index(h, *pred);
            }
        }

        if (auto pred_cluster = cluster_data_->summary.predecessor(h)) {
            auto max_element = cluster_data_->clusters.at(*pred_cluster).max();
            return index(*pred_cluster, max_element);
        }

        return min_;
    }

private:
    Node64& empty_clusters_or_tombstone(std::optional<index_t> new_min, std::optional<index_t> new_max) {
        cluster_data_.reset();
        if (new_min.has_value() && new_max.has_value()) {
            min_ = *new_min;
            max_ = *new_max;
        } else if (new_min.has_value()) {
            min_ = *new_min;
            max_ = *new_min;
        } else if (new_max.has_value()) {
            min_ = *new_max;
            max_ = *new_max;
        } else {
            min_ = std::numeric_limits<index_t>::max();
            max_ = std::numeric_limits<index_t>::min();
        }
        return *this;
    }

public:
    bool is_tombstone() const {
        return min_ > max_;
    }

    constexpr inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (!cluster_data_) return base_count;

        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            cluster_data_->values().begin(), cluster_data_->values().end(),
            base_count, std::plus<>(), [](const auto& cluster) { return cluster.size(); }
        );
    }

    constexpr inline VebTreeMemoryStats get_memory_stats() const {
        return cluster_data_ ? std::ranges::fold_left(
            cluster_data_->values(),
            [&] {
                auto stats = cluster_data_->summary.get_memory_stats();
                stats.total_clusters += cluster_data_->clusters.size();
                stats.total_nodes += 1;
                return stats;
            }(),
            [](VebTreeMemoryStats acc, const auto& cluster) {
                auto cluster_stats = cluster.get_memory_stats();
                acc.total_nodes += cluster_stats.total_nodes;
                acc.total_clusters += cluster_stats.total_clusters;
                acc.max_depth = std::max(acc.max_depth, cluster_stats.max_depth + 1);
                return acc;
            }
        ) : VebTreeMemoryStats{0, 0, 1};
    }

    Node64 clone(std::size_t& alloc) const {
        Node64 result(min_, alloc);
        result.min_ = min_;
        result.max_ = max_;

        if (cluster_data_) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            result.cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, 0, alloc);

            result.cluster_data_->summary = cluster_data_->summary.clone(alloc);
            for (const auto& [key, cluster] : cluster_data_->clusters) {
                result.cluster_data_->clusters.emplace(key, cluster.clone(alloc));
            }
        }
        return result;
    }

    Node64& or_inplace(const Node64& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (!other.cluster_data_) {
            return *this;
        }

        if (!cluster_data_) {
            auto data_alloc = tracking_allocator<cluster_data_t>(alloc);
            cluster_data_ = allocate_unique<cluster_data_t>(data_alloc, 0, alloc);
            cluster_data_->summary = other.cluster_data_->summary.clone(alloc);
            for (const auto& [key, cluster] : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(key, cluster.clone(alloc));
            }

            return *this;
        }

        cluster_data_->summary.or_inplace(other.cluster_data_->summary, alloc);
        for (const auto& [idx, other_cluster] : other.cluster_data_->clusters) {
            if (auto it = cluster_data_->clusters.find(idx); it != cluster_data_->clusters.end()) {
                it->second.or_inplace(other_cluster, alloc);
            } else {
                cluster_data_->clusters.emplace(idx, other_cluster.clone(alloc));
            }
        }
        return *this;
    }

    Node64& and_inplace(const Node64& other, std::size_t& alloc) {
        index_t potential_min = std::max(min_, other.min_);
        index_t potential_max = std::min(max_, other.max_);
        auto new_min = contains(potential_min) && other.contains(potential_min) ? std::make_optional(potential_min) : std::nullopt;
        auto new_max = contains(potential_max) && other.contains(potential_max) ? std::make_optional(potential_max) : std::nullopt;
        if (potential_min >= potential_max || !cluster_data_ || !other.cluster_data_) {
            return empty_clusters_or_tombstone(new_min, new_max);
        }

        if (cluster_data_->summary.and_inplace(other.cluster_data_->summary, alloc).is_tombstone()) {
            return empty_clusters_or_tombstone(new_min, new_max);
        }

        for (auto cluster_idx = std::make_optional(cluster_data_->summary.min()); cluster_idx.has_value(); cluster_idx = cluster_data_->summary.successor(*cluster_idx)) {
            if (auto this_it = cluster_data_->clusters.find(*cluster_idx),
                     other_it = other.cluster_data_->clusters.find(*cluster_idx);
                this_it != cluster_data_->clusters.end() &&
                other_it != other.cluster_data_->clusters.end() &&
                this_it->second.and_inplace(other_it->second, alloc).is_tombstone() &&
                (cluster_data_->clusters.erase(this_it), cluster_data_->summary.remove(*cluster_idx, alloc))
            ) {
                return empty_clusters_or_tombstone(new_min, new_max);
            }
        }

        min_ = *new_min.or_else([&] {
            auto min_cluster = cluster_data_->summary.min();
            auto min_element = cluster_data_->clusters.at(min_cluster).min();
            return std::make_optional(index(min_cluster, min_element));
        });
        max_ = *new_max.or_else([&] {
            auto max_cluster = cluster_data_->summary.max();
            auto max_element = cluster_data_->clusters.at(max_cluster).max();
            return std::make_optional(index(max_cluster, max_element));
        });

        if (max_ != potential_max && cluster_data_->clusters.at(cluster_data_->summary.max()).remove(static_cast<subindex_t>(max_), alloc)) {
            cluster_data_->summary.remove(cluster_data_->summary.max(), alloc);
        }
        if (min_ != potential_min && cluster_data_->clusters.at(cluster_data_->summary.min()).remove(static_cast<subindex_t>(min_), alloc)) {
            cluster_data_->summary.remove(cluster_data_->summary.min(), alloc);
        }

        if (cluster_data_->clusters.empty()) {
            return empty_clusters_or_tombstone(min_, max_);
        }

        return *this;
    }
};


/**
 * @brief van Emde Boas Tree with size-specific node implementations
 *
 * A van Emde Boas tree that automatically selects the appropriate node type
 * based on the universe size:
 * - Node8 for universe ≤ 256 (2^8)
 * - Node16 for universe ≤ 65536 (2^16)
 * - Node32 for universe ≤ 4294967296 (2^32)
 * - Node64 for larger universes
 */
class VebTree {
private:
    using StorageType = std::variant<std::monostate, Node8, Node16, Node32, Node64>;
    StorageType storage_;
    mutable std::size_t allocated_ = 0;

    static inline StorageType create_storage(std::size_t x, std::size_t& alloc) {
        if (x <= Node8::universe_size()) {
            return Node8{static_cast<Node8::index_t>(x), alloc};
        } else if (x <= Node16::universe_size()) {
            return Node16{0, static_cast<Node16::index_t>(x), alloc};
        } else if (x <= Node32::universe_size()) {
            return Node32{static_cast<Node32::index_t>(x), alloc};
        } else {
            return Node64{static_cast<Node64::index_t>(x), alloc};
        }
    }

    inline void grow_storage(std::size_t x, std::size_t& alloc) {
        std::visit(
            overload{
                [&](Node8&& old_storage) {
                    storage_ = Node16{std::move(old_storage), alloc};
                },
                [&](Node16&& old_storage) {
                    storage_ = Node32{std::move(old_storage), alloc};
                },
                [&](Node32&& old_storage) {
                    storage_ = Node64{std::move(old_storage), alloc};
                },
                [](auto&&) { std::unreachable(); },
            },
            std::move(storage_));

        insert(x);
    }

private:

public:
    class Iterator {
        const VebTree* tree_;
        std::size_t current_;

    public:
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = std::size_t*;
        using reference = std::size_t&;

        inline explicit Iterator(const VebTree* tree, std::size_t current)
            : tree_(tree), current_(current) {}

        inline Iterator& operator++() {
            current_ = tree_ ? tree_->successor(current_).value_or(SIZE_MAX) : SIZE_MAX;
            return *this;
        }

        inline Iterator operator++(int) {
            Iterator tmp{*this};
            ++*this;
            return tmp;
        }

        inline Iterator& operator--() {
            current_ = tree_ ? tree_->predecessor(current_).value_or(SIZE_MAX) : SIZE_MAX;
            return *this;
        }

        inline Iterator operator--(int) {
            Iterator tmp{*this};
            --*this;
            return tmp;
        }

        constexpr bool operator==(Iterator other) const {
            return current_ == other.current_;
        }

        constexpr bool operator!=(Iterator other) const {
            return !(*this == other);
        }

        constexpr std::size_t operator*() const {
            return current_;
        }
    };

    /**
     * @brief Constructs an empty VEB tree
     */
    inline explicit VebTree() : storage_{std::monostate{}}, allocated_{sizeof(*this)} {}

    inline explicit VebTree(const VebTree& other) : storage_{std::monostate{}}, allocated_{sizeof(*this)} {
        std::visit(
            overload{
                [](std::monostate) {},
                [&](const auto& s) {
                    storage_ = s.clone(allocated_);
                },
            },
            other.storage_
        );
    }

    inline VebTree(VebTree&& other) noexcept
        : storage_(std::exchange(other.storage_, std::monostate{}))
        , allocated_(std::exchange(other.allocated_, 0)) {
    }

    inline VebTree& operator=(VebTree&& other) noexcept {
        if (this != &other) {
            storage_ = std::exchange(other.storage_, std::monostate{});
            allocated_ = std::exchange(other.allocated_, 0);
        }
        return *this;
    }

    /**
     * @brief Inserts an element into the VEB tree
     * @param x The element to insert
     *
     * Time complexity: O(log log U) amortized
     */
    inline void insert(std::size_t x) {
        std::visit(
            overload{
                [&](std::monostate) {
                    storage_ = create_storage(x, allocated_);
                },
                [&](auto& s) {
                    if (x > universe_size()) {
                        grow_storage(x, allocated_);
                    } else {
                        s.insert(static_cast<index_t<decltype(s)>>(x), allocated_);
                    }
                },
            }, storage_);
    }

    /**
     * @brief Removes an element from the VEB tree
     * @param x The element to remove
     *
     * Time complexity: O(log log U)
     */
    inline void remove(std::size_t x) {
        std::visit(
            overload{
                [](std::monostate) {},
                [&](auto& s) {
                    if (x <= universe_size() && s.remove(static_cast<index_t<decltype(s)>>(x), allocated_)) {
                        storage_ = std::monostate{};
                    }
                },
            },
            storage_);
    }

    /**
     * @brief Checks if an element exists in the VEB tree
     * @param x The element to search for
     * @return true if the element exists, false otherwise
     *
     * Time complexity: O(log log U)
     */
    inline bool contains(std::size_t x) const {
        return std::visit(
            overload{
                [](std::monostate) { return false; },
                [&](const auto& s) {
                    return x <= universe_size() && s.contains(static_cast<index_t<decltype(s)>>(x));
                },
            },
            storage_);
    }

    /**
     * @brief Finds the successor of an element
     * @param x The element to find the successor of
     * @return The successor if it exists, std::nullopt otherwise
     *
     * Time complexity: O(log log U)
     */
    inline std::optional<std::size_t> successor(std::size_t x) const {
        if (x >= universe_size()) {
            return std::nullopt;
        }
        return std::visit(
            overload{
                [](std::monostate) -> std::optional<std::size_t> { return std::nullopt; },
                [&](const auto& s) -> std::optional<std::size_t> {
                    return s.successor(static_cast<index_t<decltype(s)>>(x))
                        .transform([](auto x) { return static_cast<std::size_t>(x); });
                },
            },
            storage_);
    }

    /**
     * @brief Finds the predecessor of an element
     * @param x The element to find the predecessor of
     * @return The predecessor if it exists, std::nullopt otherwise
     *
     * Time complexity: O(log log U)
     */
    inline std::optional<std::size_t> predecessor(std::size_t x) const {
        if (x > universe_size()) {
            return max();
        }
        return std::visit(
            overload{
                [](std::monostate) -> std::optional<std::size_t> { return std::nullopt; },
                [&](const auto& s) -> std::optional<std::size_t> {
                    return s.predecessor(static_cast<index_t<decltype(s)>>(x))
                        .transform([](auto x) { return static_cast<std::size_t>(x); });
                },
            },
            storage_);
    }

    /**
     * @brief Returns the minimum element
     * @return The minimum element if the tree is not empty, std::nullopt otherwise
     *
     * Time complexity: O(1)
     */
    inline std::optional<std::size_t> min() const {
        return std::visit(
            overload{
                [](std::monostate) -> std::optional<std::size_t> { return std::nullopt; },
                [&](const auto& s) -> std::optional<std::size_t> {
                    return static_cast<std::size_t>(s.min());
                },
            },
            storage_);
    }

    /**
     * @brief Returns the maximum element
     * @return The maximum element if the tree is not empty, std::nullopt otherwise
     *
     * Time complexity: O(1)
     */
    inline std::optional<std::size_t> max() const {
        return std::visit(
            overload{
                [](std::monostate) -> std::optional<std::size_t> { return std::nullopt; },
                [&](const auto& s) -> std::optional<std::size_t> {
                    return static_cast<std::size_t>(s.max());
                },
            },
            storage_);
    }

    /**
     * @brief Checks if the tree is empty
     * @return true if the tree is empty, false otherwise
     *
     * Time complexity: O(1)
     */
    constexpr bool empty() const {
        return std::holds_alternative<std::monostate>(storage_);
    }

    /**
     * @brief Clears all elements from the tree
     *
     * Time complexity: O(1)
     */
    inline void clear() {
        storage_ = std::monostate{};
    }

    /**
     * @brief Returns the number of elements in the tree
     * @return The number of elements
     *
     * Time complexity: O(n)
     */
    inline std::size_t size() const {
        return std::visit(
            overload{
                [](std::monostate) { return 0uz; },
                [](const auto& s) { return s.size(); },
            },
            storage_);
    }

    /**
     * @brief Converts the tree to a vector
     * @return A vector containing all elements
     *
     * Time complexity: O(n)
     */
    inline std::vector<std::size_t> to_vector() const {
        std::vector<std::size_t> v(begin(), end());
        std::ranges::sort(v);
        return v;
    }

    /**
     * @brief Gets memory usage statistics
     * @return Memory statistics structure
     */
    inline VebTreeMemoryStats get_memory_stats() const {
        return std::visit(
            overload{
                [](std::monostate) { return VebTreeMemoryStats{}; },
                [](const auto& s) { return s.get_memory_stats(); },
            },
            storage_);
    }

    /**
     * @brief Gets the current universe size
     * @return The current universe size
     */
    inline std::size_t universe_size() const {
        return std::visit(
            overload{
                [](std::monostate) { return 0uz; },
                [](const auto& s) { return s.universe_size(); },
            },
            storage_);
    }

    /**
     * @brief Gets the current allocated bytes
     * @return The number of bytes currently allocated by this tree
     */
    constexpr std::size_t get_allocated_bytes() const {
        return allocated_;
    }

    /**
     * @brief Iterator to the first element
     * @return Iterator to the first element
     */
    inline Iterator begin() const {
        auto min_val = min();
        return Iterator{this, min_val.value_or(SIZE_MAX)};
    }

    /**
     * @brief Iterator to the end
     * @return Iterator to the end
     */
    inline Iterator end() const {
        return Iterator{this, SIZE_MAX};
    }

    /**
     * @brief Set intersection operator
     * @param other The other VEB tree to intersect with
     * @return A new VEB tree containing elements present in both trees
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree operator&(const VebTree& other) const {
        VebTree result(*this);
        result &= other;
        return result;
    }

    /**
     * @brief Set union operator
     * @param other The other VEB tree to union with
     * @return A new VEB tree containing elements present in either tree
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree operator|(const VebTree& other) const {
        VebTree result(*this);
        result |= other;
        return result;
    }

    /**
     * @brief Set symmetric difference (XOR) operator
     * @param other The other VEB tree to XOR with
     * @return A new VEB tree containing elements present in exactly one tree
     *
     * Time complexity: O((|this| + |other|) * log log U)
     */
    inline VebTree operator^(const VebTree& other) const {
        VebTree result;
        result ^= other;
        return result;
    }

    /**
     * @brief In-place intersection operator
     * @param other The other VEB tree to intersect with
     * @return Reference to this tree after intersection
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree& operator&=(const VebTree& other) {
        if (empty() || other.empty()) {
            storage_ = std::monostate{};
            return *this;
        }

        std::visit(
            overload{
                [&](Node8& a, const Node8& b) -> void {
                    a.and_inplace(b, allocated_);
                },
                [&](Node16& a, const Node16& b) -> void {
                    a.and_inplace(b, allocated_);
                },
                [&](Node32& a, const Node32& b) -> void {
                    a.and_inplace(b, allocated_);
                },
                [&](Node64& a, const Node64& b) -> void {
                    a.and_inplace(b, allocated_);
                },
                [&](Node8& a, const Node16& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a16 = Node16{std::move(a_clone), allocated_};
                    a16.and_inplace(b, allocated_);
                    storage_ = std::move(a16);
                },
                [&](Node16& a, const Node8& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b16 = Node16{std::move(b_clone), allocated_};
                    a.and_inplace(b16, allocated_);
                },
                [&](Node8& a, const Node32& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a16 = Node16{std::move(a_clone), allocated_};
                    auto a32 = Node32{std::move(a16), allocated_};
                    a32.and_inplace(b, allocated_);
                    storage_ = std::move(a32);
                },
                [&](Node32& a, const Node8& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b16 = Node16{std::move(b_clone), allocated_};
                    auto b32 = Node32{std::move(b16), allocated_};
                    a.and_inplace(b32, allocated_);
                },
                [&](Node16& a, const Node32& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a32 = Node32{std::move(a_clone), allocated_};
                    a32.and_inplace(b, allocated_);
                    storage_ = std::move(a32);
                },
                [&](Node32& a, const Node16& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b32 = Node32{std::move(b_clone), allocated_};
                    a.and_inplace(b32, allocated_);
                },
                [&](Node8& a, const Node64& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a16 = Node16{std::move(a_clone), allocated_};
                    auto a32 = Node32{std::move(a16), allocated_};
                    auto a64 = Node64{std::move(a32), allocated_};
                    a64.and_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node8& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b16 = Node16{std::move(b_clone), allocated_};
                    auto b32 = Node32{std::move(b16), allocated_};
                    auto b64 = Node64{std::move(b32), allocated_};
                    a.and_inplace(b64, allocated_);
                },
                [&](Node16& a, const Node64& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a32 = Node32{std::move(a_clone), allocated_};
                    auto a64 = Node64{std::move(a32), allocated_};
                    a64.and_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node16& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b32 = Node32{std::move(b_clone), allocated_};
                    auto b64 = Node64{std::move(b32), allocated_};
                    a.and_inplace(b64, allocated_);
                },
                [&](Node32& a, const Node64& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a64 = Node64{std::move(a_clone), allocated_};
                    a64.and_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node32& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b64 = Node64{std::move(b_clone), allocated_};
                    a.and_inplace(b64, allocated_);
                },
                [](auto&&, auto&&) {}
            },
            storage_, other.storage_
        );

        std::visit(
            overload{
                [](std::monostate) {},
                [&](auto& node) {
                    if (node.is_tombstone()) {
                        storage_ = std::monostate{};
                    }
                },
            },
            storage_
        );

        return *this;
    }

    /**
     * @brief In-place union operator
     * @param other The other VEB tree to union with
     * @return Reference to this tree after union
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree& operator|=(const VebTree& other) {
        if (other.empty()) {
            return *this;
        }
        if (empty()) {
            storage_ = std::visit(overload{
                [](std::monostate) -> StorageType { return std::monostate{}; },
                [&](const auto& node) -> StorageType { return node.clone(allocated_); },
            }, other.storage_);
            return *this;
        }

        std::visit(
            overload{
                [&](Node8& a, const Node8& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node16& a, const Node16& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node32& a, const Node32& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node64& a, const Node64& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node8& a, const Node16& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a16 = Node16{std::move(a_clone), allocated_};
                    a16.or_inplace(b, allocated_);
                    storage_ = std::move(a16);
                },
                [&](Node16& a, const Node8& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b16 = Node16{std::move(b_clone), allocated_};
                    a.or_inplace(b16, allocated_);
                },
                [&](Node8& a, const Node32& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a16 = Node16{std::move(a_clone), allocated_};
                    auto a32 = Node32{std::move(a16), allocated_};
                    a32.or_inplace(b, allocated_);
                    storage_ = std::move(a32);
                },
                [&](Node32& a, const Node8& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b16 = Node16{std::move(b_clone), allocated_};
                    auto b32 = Node32{std::move(b16), allocated_};
                    a.or_inplace(b32, allocated_);
                },
                [&](Node16& a, const Node32& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a32 = Node32{std::move(a_clone), allocated_};
                    a32.or_inplace(b, allocated_);
                    storage_ = std::move(a32);
                },
                [&](Node32& a, const Node16& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b32 = Node32{std::move(b_clone), allocated_};
                    a.or_inplace(b32, allocated_);
                },
                [&](Node8& a, const Node64& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a16 = Node16{std::move(a_clone), allocated_};
                    auto a32 = Node32{std::move(a16), allocated_};
                    auto a64 = Node64{std::move(a32), allocated_};
                    a64.or_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node8& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b16 = Node16{std::move(b_clone), allocated_};
                    auto b32 = Node32{std::move(b16), allocated_};
                    auto b64 = Node64{std::move(b32), allocated_};
                    a.or_inplace(b64, allocated_);
                },
                [&](Node16& a, const Node64& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a32 = Node32{std::move(a_clone), allocated_};
                    auto a64 = Node64{std::move(a32), allocated_};
                    a64.or_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node16& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b32 = Node32{std::move(b_clone), allocated_};
                    auto b64 = Node64{std::move(b32), allocated_};
                    a.or_inplace(b64, allocated_);
                },
                [&](Node32& a, const Node64& b) -> void {
                    auto a_clone = a.clone(allocated_);
                    auto a64 = Node64{std::move(a_clone), allocated_};
                    a64.or_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node32& b) -> void {
                    auto b_clone = b.clone(allocated_);
                    auto b64 = Node64{std::move(b_clone), allocated_};
                    a.or_inplace(b64, allocated_);
                },
                [](auto&&, auto&&) {}
            },
            storage_, other.storage_
        );

        return *this;
    }

    /**
     * @brief In-place symmetric difference (XOR) operator
     * @param other The other VEB tree to XOR with
     * @return Reference to this tree after XOR
     */
    inline VebTree& operator^=(const VebTree& other) {
        for (std::size_t element : other) {
            if (contains(element)) {
                remove(element);
            } else {
                insert(element);
            }
        }
        return *this;
    }

    /**
     * @brief Equality comparison operator
     */
    inline bool operator==(const VebTree& other) const {
        if (size() != other.size()) return false;
        for (std::size_t element : *this) {
            if (!other.contains(element)) return false;
        }
        return true;
    }

    /**
     * @brief Inequality comparison operator
     */
    inline bool operator!=(const VebTree& other) const {
        return !(*this == other);
    }
};

#endif // VEBTREE_HPP
