/**
 * @brief van Emde Boas Tree implementation for sparse bitsets
 * reference: https://en.wikipedia.org/wiki/Van_Emde_Boas_tree
 *
 * This implementation uses a recursive node cluster structure to store the elements.
 * Each node is associated with a summary structure to indicate which clusters contain elements.
 *
 * The tree can be visualized as shown below:
 * Node<log U = 64>
 * ┌────────────────────────────────────┐
 * | min, max: u64                      | ← Lazily propagated. Not inserted into clusters.
 * | cluster_data: * {                  | ← Lazily constructed only if non-empty.
 * |   clusters: HashMap<u32, Node<32>> | ← Up to √U clusters, each of size √U.
 * |   summary : Node<32>               | ← Tracks which clusters are non-empty.
 * | }           |                      |
 * └─────────────|──────────────────────┘
 * Node<32>      ▼
 * ┌────────────────────────────────────┐
 * | min, max: u32                      |
 * | cluster_data: * {                  |
 * |   clusters: HashMap<u16, Node<16>> |
 * |   summary : Node<16>               |
 * | }           |                      |
 * └─────────────|──────────────────────┘
 * Node<16>      ▼
 * ┌────────────────────────────────────┐
 * | min, max: u16                      |
 * | cluster_data: * {                  |
 * |   summary : Node<8>                | ← Used to index into clusters in constant time. Requires sorted clusters.
 * |   clusters: Array<Node<8>, 0>      | ← Up to 256 elements. FAM is more cache-friendly than HashMap.
 * | }           |                      |
 * └─────────────|──────────────────────┘
 * Node<8>       ▼
 * ┌────────────────────────────────────┐
 * | bits: Array<u64, 4>                | ← 256 bits, SIMD-friendly.
 * └────────────────────────────────────┘
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
#include <utility>
#include <variant>
#include <vector>

#include "allocator/tracking_allocator.hpp"
#include "allocator/allocate_unique.hpp"

template<typename Key, typename Value, template<typename...> typename HM>
using HashMap = HM<Key, Value, std::hash<Key>, std::equal_to<Key>, tracking_allocator<std::pair<const Key, Value>>>;

struct StdManager {
    template<typename Key, typename Value>
    using HashMap_t = HashMap<Key, Value, std::unordered_map>;
    static const char* name() { return "std::unordered_map"; }
};

#ifdef HAVE_ABSL
#include <absl/container/flat_hash_map.h>
struct AbslManager {
    template<typename Key, typename Value>
    using HashMap_t = HashMap<Key, Value, absl::flat_hash_map>;
    static const char* name() { return "absl::flat_hash_map"; }
};
#endif

#ifdef HAVE_BOOST
#include <boost/unordered/unordered_flat_map.hpp>
#include <boost/unordered/unordered_node_map.hpp>
#include <boost/unordered_map.hpp>
struct BoostFlatManager {
    template<typename Key, typename Value>
    using HashMap_t = HashMap<Key, Value, boost::unordered_flat_map>;
    static const char* name() { return "boost::unordered_flat_map"; }
};

struct BoostNodeManager {
    template<typename Key, typename Value>
    using HashMap_t = HashMap<Key, Value, boost::unordered_node_map>;
    static const char* name() { return "boost::unordered_node_map"; }
};

struct BoostManager {
    template<typename Key, typename Value>
    using HashMap_t = HashMap<Key, Value, boost::unordered_map>;
    static const char* name() { return "boost::unordered_map"; }
};
#endif

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
    template<typename Manager> friend class VebTree;
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
                return Node8::index(word, static_cast<subindex_t>(std::countr_zero(bits_[word])));
            }
        }
        std::unreachable();
    }

    constexpr inline index_t max() const {
        for (subindex_t word = 4; word > 0; --word) {
            if (bits_[word - 1] != 0) {
                return Node8::index(word - 1, static_cast<subindex_t>(63 - std::countl_zero(bits_[word - 1])));
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
        if (x >= max()) {
            return std::nullopt;
        }

        const auto [start_word, start_bit] = decompose(x);

        std::uint64_t word = 0;
        if (start_bit + 1 < 64) {
            word = bits_[start_word] & (~0ULL << (start_bit + 1));
        }
        if (word != 0) {
            return Node8::index(start_word, static_cast<subindex_t>(std::countr_zero(word)));
        }

        for (subindex_t word_idx = start_word + 1; word_idx < 4; ++word_idx) {
            if (bits_[word_idx] != 0) {
                return Node8::index(word_idx, static_cast<subindex_t>(std::countr_zero(bits_[word_idx])));
            }
        }

        return std::nullopt;
    }

    constexpr inline std::optional<index_t> predecessor(index_t x) const {
        if (x <= min()) {
            return std::nullopt;
        }
        if (x > max()) {
            return max();
        }

        const auto [start_word, start_bit] = decompose(x - 1);

        std::uint64_t word = bits_[start_word] & (start_bit == 63 ? -1ULL : ((1ULL << (start_bit + 1)) - 1));
        if (word != 0) {
            return Node8::index(start_word, static_cast<subindex_t>(63 - std::countl_zero(word)));
        }

        for (subindex_t word_idx = start_word; word_idx > 0; --word_idx) {
            if (bits_[word_idx - 1] != 0) {
                return Node8::index(word_idx - 1, static_cast<subindex_t>(63 - std::countl_zero(bits_[word_idx - 1])));
            }
        }

        return std::nullopt;
    }

    constexpr inline std::size_t size() const {
        return std::transform_reduce(
            bits_.begin(), bits_.end(), 0uz,
            std::plus<>(), [](std::uint64_t word) { return std::popcount(word); }
        );
    }

    constexpr VebTreeMemoryStats get_memory_stats() const {
        return {0, 0, 1};
    }
};

class Node16 {
    template<typename Manager> friend class VebTree;
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

    inline void grow_capacity() {
        const std::uint16_t new_capacity = static_cast<std::uint16_t>(std::min(256, capacity_ + (capacity_ >> 2) + 1));
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
            grow_capacity();
        }
        if (idx < size) {
            std::copy(cluster_data_->clusters_ + idx, cluster_data_->clusters_ + size, cluster_data_->clusters_ + idx + 1);
        }
        cluster_data_->clusters_[idx] = subnode_t(lo, alloc);
        cluster_data_->summary_.insert(hi, alloc);
    }

public:
    inline explicit Node16(index_t x, std::size_t& alloc)
        : cluster_data_(nullptr), alloc_(alloc), capacity_(0), min_(x), max_(x) {
    }

    inline Node16(Node8&& old_storage, std::size_t& alloc)
        : cluster_data_(nullptr)
        , alloc_(alloc)
        , capacity_(0)
        , min_(old_storage.min())
        , max_(old_storage.max())
    {
        auto old_min = static_cast<Node8::index_t>(min_);
        auto old_max = static_cast<Node8::index_t>(max_);

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

    Node16(const Node16&) = delete;
    Node16& operator=(const Node16&) = delete;

    Node16(Node16&& other) noexcept
        : cluster_data_(std::exchange(other.cluster_data_, nullptr))
        , alloc_(other.alloc_)
        , capacity_(std::exchange(other.capacity_, 0))
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
                x = min_ = Node16::index(min_cluster, min_element);
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
                x = max_ = Node16::index(max_cluster, max_element);
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
                    return Node16::index(h, *succ);
                }
            }
        }

        if (auto succ_cluster = cluster_data_->summary_.successor(h)) {
            auto min_element = cluster_data_->clusters_[cluster_data_->index_of(*succ_cluster)].min();
            return Node16::index(*succ_cluster, min_element);
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
                    return Node16::index(h, *pred);
                }
            }
        }

        if (auto pred_cluster = cluster_data_->summary_.predecessor(h)) {
            auto max_element = cluster_data_->clusters_[cluster_data_->index_of(*pred_cluster)].max();
            return Node16::index(*pred_cluster, max_element);
        }

        return min_;
    }

    constexpr inline std::size_t size() const {
        std::size_t total = (min_ == max_) ? 1uz : 2uz;

        if (cluster_data_) {
            const std::size_t cluster_count = cluster_data_->size();
            for (std::size_t i = 0; i < cluster_count; ++i) {
                total += cluster_data_->clusters_[i].size();
            }
        }

        return total;
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
};

template<typename Manager>
class Node32 {
    template<typename M> friend class VebTree;
public:
    using subnode_t = Node16;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint32_t;

private:
    struct cluster_data_t {
        using cluster_map_t = typename Manager::template HashMap_t<subindex_t, subnode_t>;
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
        auto old_min = static_cast<Node16::index_t>(min_);
        auto old_max = static_cast<Node16::index_t>(max_);

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
                x = min_ = Node32::index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (!cluster_data_ || cluster_data_->clusters.empty()) {
                max_ = min_;
            } else {
                auto max_cluster = cluster_data_->summary.max();
                auto max_element = cluster_data_->clusters.at(max_cluster).max();
                x = max_ = Node32::index(max_cluster, max_element);
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
                return Node32::index(h, *succ);
            }
        }

        if (auto succ_cluster = cluster_data_->summary.successor(h)) {
            auto min_element = cluster_data_->clusters.at(*succ_cluster).min();
            return Node32::index(*succ_cluster, min_element);
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
                return Node32::index(h, *pred);
            }
        }

        if (auto pred_cluster = cluster_data_->summary.predecessor(h)) {
            auto max_element = cluster_data_->clusters.at(*pred_cluster).max();
            return Node32::index(*pred_cluster, max_element);
        }

        return min_;
    }

    inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (!cluster_data_) return base_count;

        return std::ranges::fold_left(
            cluster_data_->values(),
            base_count,
            [](std::size_t acc, const auto& cluster) { return acc + cluster.size(); }
        );
    }

    inline VebTreeMemoryStats get_memory_stats() const {
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
};

template<typename Manager>
class Node64 {
    template<typename M> friend class VebTree;
public:
    using subnode_t = Node32<Manager>;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint64_t;

private:
    struct cluster_data_t {
        using cluster_map_t = typename Manager::template HashMap_t<subindex_t, subnode_t>;
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

    inline Node64(Node32<Manager>&& old_storage, std::size_t& alloc)
        : cluster_data_{nullptr, AllocDeleter{tracking_allocator<cluster_data_t>(alloc)}}
        , min_(old_storage.min())
        , max_(old_storage.max())
    {
        auto old_min = static_cast<typename Node32<Manager>::index_t>(min_);
        auto old_max = static_cast<typename Node32<Manager>::index_t>(max_);

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
                x = min_ = Node64::index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (!cluster_data_ || cluster_data_->clusters.empty()) {
                max_ = min_;
            } else {
                auto max_cluster = cluster_data_->summary.max();
                auto max_element = cluster_data_->clusters.at(max_cluster).max();
                x = max_ = Node64::index(max_cluster, max_element);
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
                return Node64::index(h, *succ);
            }
        }

        if (auto succ_cluster = cluster_data_->summary.successor(h)) {
            auto min_element = cluster_data_->clusters.at(*succ_cluster).min();
            return Node64::index(*succ_cluster, min_element);
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
                return Node64::index(h, *pred);
            }
        }

        if (auto pred_cluster = cluster_data_->summary.predecessor(h)) {
            auto max_element = cluster_data_->clusters.at(*pred_cluster).max();
            return Node64::index(*pred_cluster, max_element);
        }

        return min_;
    }

    constexpr inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (!cluster_data_) return base_count;

        return std::ranges::fold_left(
            cluster_data_->values(),
            base_count,
            [](std::size_t acc, const auto& cluster) { return acc + cluster.size(); }
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
 *
 * @tparam Manager The hash table manager class that defines which
 *                 hash table implementation to use
 */
template<typename Manager = StdManager>
class VebTree {
private:
    using StorageType = std::variant<std::monostate, Node8, Node16, Node32<Manager>, Node64<Manager>>;
    StorageType storage_;
    mutable std::size_t allocated_ = 0;

    static inline StorageType create_storage(std::size_t x, std::size_t& alloc) {
        if (x <= Node8::universe_size()) {
            return Node8{static_cast<Node8::index_t>(x), alloc};
        } else if (x <= Node16::universe_size()) {
            return Node16{static_cast<Node16::index_t>(x), alloc};
        } else if (x <= Node32<Manager>::universe_size()) {
            return Node32<Manager>{static_cast<typename Node32<Manager>::index_t>(x), alloc};
        } else {
            return Node64<Manager>{static_cast<typename Node64<Manager>::index_t>(x), alloc};
        }
    }

    inline void grow_storage(std::size_t x, std::size_t& alloc) {
        std::visit(
            overload{
                [&](Node8&& old_storage) {
                    storage_ = Node16{std::move(old_storage), alloc};
                },
                [&](Node16&& old_storage) {
                    storage_ = Node32<Manager>{std::move(old_storage), alloc};
                },
                [&](Node32<Manager>&& old_storage) {
                    storage_ = Node64<Manager>{std::move(old_storage), alloc};
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
     * @brief Returns the name of the hash table implementation being used
     */
    static const char* hash_table_name() {
        return Manager::name();
    }

    /**
     * @brief Constructs an empty VEB tree
     */
    inline explicit VebTree() : storage_{std::monostate{}}, allocated_{sizeof(*this)} {}

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
                [&](const auto& s) { return s.size(); },
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
                [&](const auto& s) { return s.get_memory_stats(); },
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
                [&](const auto& s) { return s.universe_size(); },
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
     * Time complexity: O(min(|this|, |other|) * log log U)
     */
    inline VebTree operator&(const VebTree& other) const {
        VebTree result;
        if (empty() || other.empty()) return result;

        const auto& [smaller, larger] = (size() <= other.size())
            ? std::pair{this, &other} : std::pair{&other, this};

        for (std::size_t element : *smaller) {
            if (larger->contains(element)) {
                result.insert(element);
            }
        }
        return result;
    }

    /**
     * @brief Set union operator
     * @param other The other VEB tree to union with
     * @return A new VEB tree containing elements present in either tree
     *
     * Time complexity: O((|this| + |other|) * log log U)
     */
    inline VebTree operator|(const VebTree& other) const {
        VebTree result;
        for (std::size_t element : *this) {
            result.insert(element);
        }
        for (std::size_t element : other) {
            result.insert(element);
        }
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
        for (std::size_t element : *this) {
            if (!other.contains(element)) {
                result.insert(element);
            }
        }
        for (std::size_t element : other) {
            if (!contains(element)) {
                result.insert(element);
            }
        }
        return result;
    }

    /**
     * @brief In-place intersection operator
     * @param other The other VEB tree to intersect with
     * @return Reference to this tree after intersection
     */
    inline VebTree& operator&=(const VebTree& other) {
        for (std::size_t element : *this) {
            if (!other.contains(element)) {
                remove(element);
            }
        }
        return *this;
    }

    /**
     * @brief In-place union operator
     * @param other The other VEB tree to union with
     * @return Reference to this tree after union
     */
    inline VebTree& operator|=(const VebTree& other) {
        for (std::size_t element : other) {
            insert(element);
        }
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

template class VebTree<StdManager>;
using StdVebTree = VebTree<StdManager>;

#ifdef HAVE_ABSL
template class VebTree<AbslManager>;
using AbslVebTree = VebTree<AbslManager>;
#endif

#ifdef HAVE_BOOST
template class VebTree<BoostFlatManager>;
using BoostFlatVebTree = VebTree<BoostFlatManager>;

template class VebTree<BoostNodeManager>;
using BoostNodeVebTree = VebTree<BoostNodeManager>;

template class VebTree<BoostManager>;
using BoostVebTree = VebTree<BoostManager>;
#endif

#endif // VEBTREE_HPP
