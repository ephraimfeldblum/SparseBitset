#ifndef NODE16_HPP
#define NODE16_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>
#include <cstring>
#include <numeric>
#include <execution>
#include <functional>
#include <ranges>
#include "node8.hpp"
#include "VebCommon.hpp"
#include "allocator/tracking_allocator.hpp"

/* Node16:
 * Represents a van Emde Boas tree node for universe size up to 2^16.
 * This node uses `uint16_t` as its index type and `Node8` as its subnodes.
 *
 * The layout of this node is as follows:
 *   - A pointer to a `cluster_data_t` structure containing:
 *       - An instance of a subnode, which serves as the summary.
 *       - An array of subnodes representing individual clusters.
 *   - Two index fields (`min_` and `max_`) to lazily propagate the minimum and maximum elements.
 *   - An index field (`key_`) to identify which of the parent `Node32` clusters this node belongs to.
 *   - A `capacity_` field to track the allocated size of the clusters array.
 * 
 * The total size of this class is 16 bytes on 64-bit systems, ie two registers. As such, we should prefer passing
 *   instances of this class by value whenever possible.
 * The purpose of this design is to optimize memory usage while maintaining fast operations on the underlying nodes.
 * The `cluster_data_t` structure is allocated dynamically to allow for flexible sizing of the clusters array.
 * The `capacity_` field helps manage the dynamic array of clusters, allowing for efficient resizing when necessary.
 * Growing the capacity involves allocating a new array, copying existing clusters, and updating the pointer.
 * The growth strategy increases capacity by 25% plus one to balance between memory overhead and allocation frequency.
 * This design balances memory efficiency with performance, making it suitable for fast set operations.
 * The `min_` and `max_` fields enable quick access to the minimum and maximum elements without traversing the entire
 *   structure.
 * Additionally, the `key_` field allows us to efficiently determine which `Node32` cluster this node belongs to during
 *   operations like insertion and deletion.
 */
class Node16 {
    friend class VebTree;
public:
    using subnode_t = Node8;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint16_t;
    using allocator_t = tracking_allocator<subnode_t>;

private:
    struct cluster_data_t {
        subnode_t summary_;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        subnode_t clusters_[];
#pragma GCC diagnostic pop

        constexpr inline subindex_t index_of(subindex_t x) const {
            return summary_.get_cluster_index(x);
        }
        constexpr inline subnode_t* find(subindex_t x) {
            return summary_.contains(x) ? &clusters_[index_of(x)] : nullptr;
        }
        constexpr inline const subnode_t* find(subindex_t x) const {
            return summary_.contains(x) ? &clusters_[index_of(x)] : nullptr;
        }
        constexpr inline std::size_t size() const {
            return summary_.size();
        }
    };

    cluster_data_t* cluster_data_{};
    std::uint16_t capacity_{};
    index_t key_{};
    index_t min_{};
    index_t max_{};

    static constexpr inline std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 8), static_cast<subindex_t>(x)};
    }
    static constexpr inline index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 8 | low;
    }

    constexpr inline subnode_t* find(subindex_t x) {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }
    constexpr inline const subnode_t* find(subindex_t x) const {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }

    constexpr inline void grow_capacity_if_needed(std::size_t& alloc) {
        const std::size_t size{cluster_data_ ? cluster_data_->size() : 0};
        if (size < capacity_) {
            return;
        }
        const std::uint16_t new_capacity{static_cast<std::uint16_t>(std::min(256, capacity_ + (capacity_ / 4) + 1))};
        allocator_t a{alloc};
        auto* new_data{reinterpret_cast<cluster_data_t*>(a.allocate(new_capacity + 1))};
        new_data->summary_ = cluster_data_->summary_;
        std::copy_n(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            cluster_data_->clusters_, cluster_data_->size(), new_data->clusters_
        );
        destroy(alloc);
        cluster_data_ = new_data;
        capacity_ = new_capacity;
    }

    constexpr inline void emplace(subindex_t hi, subindex_t lo, std::size_t& alloc) {
        if (cluster_data_ == nullptr) {
            allocator_t a{alloc};
            cluster_data_ = reinterpret_cast<cluster_data_t*>(a.allocate(2));
            cluster_data_->summary_ = subnode_t{hi};
            cluster_data_->clusters_[0] = subnode_t{lo};
            capacity_ = 1;
            return;
        }

        auto& summary{cluster_data_->summary_};
        auto* clusters{cluster_data_->clusters_};

        const std::uint8_t idx{cluster_data_->index_of(hi)};
        if (summary.contains(hi)) {
            clusters[idx].insert(lo);
            return;
        }

        grow_capacity_if_needed(alloc);
        if (const std::size_t size{cluster_data_->size()}; idx < size) {
            const auto begin{clusters + idx};
            const auto end{clusters + size};
            std::move_backward(begin, end, end);
        }
        clusters[idx] = subnode_t{lo};
        summary.insert(hi);
    }

public:
    constexpr inline explicit Node16(index_t hi, index_t lo)
        : cluster_data_{nullptr}, capacity_{0}, key_{hi}, min_{lo}, max_{lo} {
    }

   constexpr inline Node16(Node8 old_storage, std::size_t& alloc)
        : cluster_data_{nullptr}
        , capacity_{0}
        , key_{0}
        , min_{old_storage.min()}
        , max_{old_storage.max()}
    {
        const auto old_min{old_storage.min()};
        const auto old_max{old_storage.max()};

        old_storage.remove(old_min);
        if (old_min != old_max) {
            old_storage.remove(old_max);
        }

        if (old_storage.size() > 0) {
            allocator_t a{alloc};
            cluster_data_ = reinterpret_cast<cluster_data_t*>(a.allocate(2));
            cluster_data_->summary_ = subnode_t{0};
            cluster_data_->clusters_[0] = old_storage;
            capacity_ = 1;
        }
    }

    constexpr inline void destroy(std::size_t& alloc) {
        if (cluster_data_ != nullptr) {
            allocator_t a{alloc};
            a.deallocate(reinterpret_cast<subnode_t*>(cluster_data_), capacity_ + 1);
            cluster_data_ = nullptr;
            capacity_ = 0;
        }
    }

    constexpr inline Node16 clone(std::size_t& alloc) const {
        Node16 result{key_, min_};
        result.max_ = max_;

        if (cluster_data_ != nullptr) {
            allocator_t a{alloc};
            const auto size{cluster_data_->size()};
            result.cluster_data_ = reinterpret_cast<cluster_data_t*>(a.allocate(size + 1));
            result.cluster_data_->summary_ = cluster_data_->summary_;
            std::copy_n(
#ifdef __cpp_lib_execution
                std::execution::unseq,
#endif
                cluster_data_->clusters_, size, result.cluster_data_->clusters_
            );
            result.capacity_ = static_cast<std::uint16_t>(size);
        }
        return result;
    }

    constexpr inline Node16(Node16&& other) noexcept
        : cluster_data_{std::exchange(other.cluster_data_, nullptr)}
        , capacity_{std::exchange(other.capacity_, 0)}
        , key_{other.key_}
        , min_{other.min_}
        , max_{other.max_} {
    }

    constexpr inline Node16& operator=(Node16&& other) noexcept {
        if (this != &other) {
            cluster_data_ = std::exchange(other.cluster_data_, nullptr);
            capacity_ = std::exchange(other.capacity_, 0);
            key_ = other.key_;
            min_ = other.min_;
            max_ = other.max_;
        }
        return *this;
    }

    // Node16 is non-copyable
    Node16(const Node16& other) = delete;
    Node16& operator=(const Node16&) = delete;

    // Node16 must be destructed via `.destroy()`. Failure to do so will result in UB.
    // ~Node16() noexcept = default;

    static constexpr inline std::size_t universe_size() { return std::numeric_limits<index_t>::max(); }
    constexpr inline index_t min() const { return min_; }
    constexpr inline index_t max() const { return max_; }

    constexpr inline void insert(index_t x, std::size_t& alloc) {
        if (x < min_) {
            std::swap(x, min_);
        }
        if (x > max_) {
            std::swap(x, max_);
        }
        if (x == min_ || x == max_) {
            return;
        }

        const auto [h, l] {decompose(x)};
        emplace(h, l, alloc);
    }

    constexpr inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (!cluster_data_ || cluster_data_->size() == 0) {
                if (max_ == min_) {
                    return true;
                } else {
                    min_ = max_;
                    return false;
                }
            } else {
                auto min_cluster{cluster_data_->summary_.min()};
                auto min_element{cluster_data_->clusters_[cluster_data_->index_of(min_cluster)].min()};
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
                auto max_cluster{cluster_data_->summary_.max()};
                auto max_element{cluster_data_->clusters_[cluster_data_->index_of(max_cluster)].max()};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        if (auto* cluster{find(h)}; cluster != nullptr) {
            if (cluster->remove(l)) {
                const std::uint8_t idx{cluster_data_->index_of(h)};
                const std::size_t size{cluster_data_->size()};
                const auto begin{cluster_data_->clusters_ + idx + 1};
                const auto end{cluster_data_->clusters_ + size};
                std::move(begin, end, begin - 1);

                if (cluster_data_->summary_.remove(h) && cluster_data_->size() == 0) {
                    destroy(alloc);
                }
            }
        }

        return false;
    }

    constexpr inline bool contains(index_t x) const {
        if (x == min_ || x == max_) {
            return true;
        }

        const auto [h, l] {decompose(x)};
        if (const auto* cluster{find(h)}; cluster != nullptr) {
            return cluster->contains(l);
        }
        return false;
    }

    constexpr inline std::optional<index_t> successor(index_t x) const {
        if (x < min_) {
            return min_;
        }
        if (x >= max_) {
            return std::nullopt;
        }

        if (cluster_data_ == nullptr) {
            return std::make_optional(max_);
        }

        const auto [h, l] {decompose(x)};

        if (const auto* cluster{find(h)}; cluster != nullptr) {
            if (l < cluster->max()) {
                if (auto succ{cluster->successor(l)}; succ.has_value()) {
                    return index(h, *succ);
                }
            }
        }

        if (auto succ_cluster{cluster_data_->summary_.successor(h)}; succ_cluster.has_value()) {
            auto min_element{cluster_data_->clusters_[cluster_data_->index_of(*succ_cluster)].min()};
            return index(*succ_cluster, min_element);
        }

        return std::make_optional(max_);
    }

    constexpr inline std::optional<index_t> predecessor(index_t x) const {
        if (x > max_) {
            return max_;
        }
        if (x <= min_) {
            return std::nullopt;
        }

        if (cluster_data_ == nullptr) return min_;

        const auto [h, l] {decompose(x)};

        if (const auto* cluster{find(h)}; cluster != nullptr) {
            if (l > cluster->min()) {
                if (auto pred{cluster->predecessor(l)}; pred.has_value()) {
                    return index(h, *pred);
                }
            }
        }

        if (auto pred_cluster{cluster_data_->summary_.predecessor(h)}; pred_cluster.has_value()) {
            auto max_element{cluster_data_->clusters_[cluster_data_->index_of(*pred_cluster)].max()};
            return index(*pred_cluster, max_element);
        }

        return min_;
    }

    constexpr inline std::size_t size() const {
        const std::size_t base_count = (min_ == max_) ? 1uz : 2uz;
        if (cluster_data_ == nullptr) {
            return base_count;
        }

        const auto* const data {cluster_data_->clusters_};
        const auto count{cluster_data_->size()};
        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            data, data + count, base_count, std::plus<>{},
            [](const auto& cluster) { return cluster.size(); }
        );
    }

    constexpr inline VebTreeMemoryStats get_memory_stats() const {
        if (cluster_data_ == nullptr) {
            return {0, 0, 1};
        }

        auto stats{cluster_data_->summary_.get_memory_stats()};
        stats.total_nodes += 1;
        stats.max_depth += 1;
        stats.total_clusters += cluster_data_->size();

        const std::size_t cluster_count = cluster_data_->size();
        for (std::size_t i = 0; i < cluster_count; ++i) {
            const auto cluster_stats{cluster_data_->clusters_[i].get_memory_stats()};
            stats.total_nodes += cluster_stats.total_nodes;
            stats.max_depth = std::max(stats.max_depth, cluster_stats.max_depth + 1);
        }

        return stats;
    }

    constexpr inline decltype(auto) empty_clusters_or_tombstone(this auto&& self, std::optional<index_t> new_min, std::optional<index_t> new_max, std::size_t& alloc) {
        self.destroy(alloc);
        if (new_min.has_value() && new_max.has_value()) {
            self.min_ = *new_min;
            self.max_ = *new_max;
        } else if (new_min.has_value()) {
            self.min_ = *new_min;
            self.max_ = *new_min;
        } else if (new_max.has_value()) {
            self.min_ = *new_max;
            self.max_ = *new_max;
        } else {
            self.min_ = std::numeric_limits<index_t>::max();
            self.max_ = std::numeric_limits<index_t>::min();
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr inline bool is_tombstone() const {
        return min_ > max_;
    }

    constexpr inline decltype(auto) or_inplace(this auto&& self, const Node16& other, std::size_t& alloc) {
        self.insert(other.min_, alloc);
        self.insert(other.max_, alloc);

        if (other.cluster_data_ == nullptr) {
            return std::forward<decltype(self)>(self);
        }

        allocator_t a{alloc};
        if (self.cluster_data_ == nullptr) {
            const std::size_t size = other.cluster_data_->size();
            self.cluster_data_ = reinterpret_cast<cluster_data_t*>(a.allocate(size + 1));
            self.cluster_data_->summary_ = other.cluster_data_->summary_;
            std::copy_n(
#ifdef __cpp_lib_execution
                std::execution::unseq,
#endif
                other.cluster_data_->clusters_, size, self.cluster_data_->clusters_
            );
            self.capacity_ = static_cast<std::uint16_t>(size);

            return std::forward<decltype(self)>(self);
        }

        auto& this_summary{self.cluster_data_->summary_};
        auto* this_clusters{self.cluster_data_->clusters_};
        const auto& other_summary{other.cluster_data_->summary_};
        const auto* other_clusters{other.cluster_data_->clusters_};

        if (auto merge_summary{auto{this_summary}.or_inplace(other_summary)};
            merge_summary.size() != self.cluster_data_->size()
        ) {
            auto new_cluster_data{reinterpret_cast<cluster_data_t*>(a.allocate(merge_summary.size() + 1))};
            auto new_capacity{static_cast<std::uint16_t>(merge_summary.size())};
            auto& new_summary{new_cluster_data->summary_};
            auto* new_clusters{new_cluster_data->clusters_};
            new_summary = std::move(merge_summary);

            std::size_t i{};
            std::size_t j{};
            std::size_t k{};
            for (auto idx{std::make_optional(new_summary.min())}; idx.has_value(); idx = new_summary.successor(*idx)) {
                const bool in_this = this_summary.contains(*idx);
                const bool in_other = other_summary.contains(*idx);
                if (in_this && in_other) {
                    new_clusters[k++] = this_clusters[i++].or_inplace(other_clusters[j++]); 
                } else if (in_this) {
                    new_clusters[k++] = this_clusters[i++];
                } else if (in_other) {
                    new_clusters[k++] = other_clusters[j++];
                } else {
                    std::unreachable();
                }
            }
            self.destroy(alloc);
            self.cluster_data_ = new_cluster_data;
            self.capacity_ = new_capacity;
            return std::forward<decltype(self)>(self);
        }

        std::size_t i{};
        std::size_t j{};
        for (auto idx{std::make_optional(this_summary.min())}; idx.has_value(); idx = this_summary.successor(*idx)) {
            if (other_summary.contains(*idx)) {
                self.cluster_data_->clusters_[i].or_inplace(other.cluster_data_->clusters_[j]);
                ++j;
            }
            ++i;
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr inline decltype(auto) and_inplace(this auto&& self, const Node16& other, std::size_t& alloc) {
        if (self.cluster_data_ == nullptr || other.cluster_data_ == nullptr) {
            return self.empty_clusters_or_tombstone(std::nullopt, std::nullopt, alloc);
        }

        const index_t potential_min{std::max(self.min_, other.min_)};
        const index_t potential_max{std::min(self.max_, other.max_)};
        if (potential_min >= potential_max) {
            return self.empty_clusters_or_tombstone(std::nullopt, std::nullopt, alloc);
        }

        auto& this_summary{self.cluster_data_->summary_};
        auto* this_clusters{self.cluster_data_->clusters_};
        const auto& other_summary{other.cluster_data_->summary_};
        const auto* other_clusters{other.cluster_data_->clusters_};

        const auto new_min{self.contains(potential_min) && other.contains(potential_min) ? std::make_optional(potential_min) : std::nullopt};
        const auto new_max{self.contains(potential_max) && other.contains(potential_max) ? std::make_optional(potential_max) : std::nullopt};

        auto summary_intersection{auto{this_summary}.and_inplace(other_summary)};
        if (summary_intersection.is_tombstone()) {
            return self.empty_clusters_or_tombstone(new_min, new_max, alloc);
        }

        std::size_t write_idx{};
        for (auto cluster_idx{std::make_optional(summary_intersection.min())}; cluster_idx.has_value(); cluster_idx = summary_intersection.successor(*cluster_idx)) {
            const subindex_t this_cluster_pos{self.cluster_data_->index_of(*cluster_idx)};
            const subindex_t other_cluster_pos{other.cluster_data_->index_of(*cluster_idx)};
            auto& this_cluster{this_clusters[this_cluster_pos]};
            const auto& other_cluster{other_clusters[other_cluster_pos]};

            if (!this_cluster.and_inplace(other_cluster).is_tombstone()) {
                if (write_idx != this_cluster_pos) {
                    this_clusters[write_idx] = this_cluster;
                }
                write_idx++;
            } else if (summary_intersection.remove(*cluster_idx)) {
                return self.empty_clusters_or_tombstone(new_min, new_max, alloc);
            }
        }
        this_summary = summary_intersection;

        self.max_ = new_max.has_value() ? *new_max : index(this_summary.max(), this_clusters[this_summary.size() - 1].max());
        if (self.max_ != potential_max && this_clusters[this_summary.size() - 1].remove(static_cast<subindex_t>(self.max_))) {
            this_summary.remove(this_summary.max());
        }
        self.min_ = new_min.has_value() ? *new_min : index(this_summary.min(), this_clusters[0].min());
        if (self.min_ != potential_min && this_clusters[0].remove(static_cast<subindex_t>(self.min_))) {
            this_summary.remove(this_summary.min());
            const auto begin{this_clusters + 1};
            const auto end{this_clusters + write_idx};
            std::move(begin, end, begin - 1);
        }
        if (this_summary.size() == 0) {
            return self.empty_clusters_or_tombstone(self.min_, self.max_, alloc);
        }

        return std::forward<decltype(self)>(self);
    }
    
    struct Eq {
        using is_transparent = void;
        constexpr inline bool operator()(const Node16& lhs, const Node16& rhs) const {
            return lhs.key_ == rhs.key_;
        }
        constexpr inline bool operator()(const Node16& lhs, const Node16::index_t rhs) const {
            return lhs.key_ == rhs;
        }
        constexpr inline bool operator()(const Node16::index_t lhs, const Node16& rhs) const {
            return lhs == rhs.key_;
        }
        constexpr inline bool operator()(const Node16::index_t lhs, const Node16::index_t rhs) const {
            return lhs == rhs;
        }
    };
    struct Hash {
        using is_transparent = void;
        constexpr inline std::size_t operator()(const Node16& node) const {
            return std::hash<Node16::index_t>{}(node.key_);
        }
        constexpr inline std::size_t operator()(const Node16::index_t key) const {
            return std::hash<Node16::index_t>{}(key);
        }
    };
};

#endif // NODE16_HPP
