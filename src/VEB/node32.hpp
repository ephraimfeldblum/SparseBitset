#ifndef NODE32_HPP
#define NODE32_HPP

#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <utility>
#include <algorithm>
#include <ranges>
#include <numeric>
#include <execution>
#include <functional>
#include "node16.hpp"
#include "VebCommon.hpp"
#include "allocator/tracking_allocator.hpp"

/* Node32:
 * Represents a van Emde Boas tree node for universe size up to 2^32.
 * This node uses `uint32_t` as its index type and `Node16` as its subnodes.
 *
 * The layout of this node is as follows:
 *   - A pointer to a `cluster_data_t` structure containing:.
 *       - An instance of a subnode serving as the summary.
 *       - A hash map of subnodes representing individual clusters.
 *         - The map is implemented as an `unordered_set` with keys inlined in the nodes to optimize memory usage.
 *   - Two index fields (`min_` and `max_`) to lazily propagate the minimum and maximum elements.
 *
 * The purpose of this design is to optimize memory usage while maintaining fast operations on the underlying nodes.
 * The total size of this class is 16 bytes on 64-bit systems, ie two registers. As such, we should prefer passing
 *   instances of this class by value whenever possible.
 */
class Node32 {
    friend class VebTree;
public:
    using subnode_t = Node16;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint32_t;

private:
    struct cluster_data_t {
        using key_type = subnode_t;
        using hash_t = key_type::Hash;
        using key_equal_t = key_type::Eq;
        using allocator_t = tracking_allocator<key_type>;
        using cluster_map_t = std::unordered_set<key_type, hash_t, key_equal_t, allocator_t>;

        cluster_map_t clusters;
        subnode_t summary;

        cluster_data_t(subindex_t x, std::size_t& alloc)
            : clusters{allocator_t{alloc}}
            , summary{0, x} {
        }
    };
    using allocator_t = tracking_allocator<cluster_data_t>;

    cluster_data_t* cluster_data_{};
    index_t min_{};
    index_t max_{};

    static constexpr inline std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 16), static_cast<subindex_t>(x)};
    }
    static constexpr inline index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 16 | low;
    }

public:
    constexpr inline explicit Node32(index_t x)
        : min_{x}, max_{x} {
    }

    constexpr inline Node32(Node16&& old_storage, std::size_t& alloc)
        : cluster_data_{nullptr}
        , min_{old_storage.min()}
        , max_{old_storage.max()}
    {
        auto old_min{old_storage.min()};
        auto old_max{old_storage.max()};

        old_storage.remove(old_min, alloc);
        if (old_min != old_max) {
            old_storage.remove(old_max, alloc);
        }

        if (old_storage.size() > 0) {
            allocator_t a{alloc};
            cluster_data_ = a.allocate(1);
            a.construct(cluster_data_, 0, alloc);
            cluster_data_->clusters.emplace(std::move(old_storage));
        }
    }

    // Node32 is non-copyable. Copying would require deep copies of potentially large structures.
    // If you need to make a copy, use `.clone(alloc)` instead.
    Node32(const Node32& other) = delete;
    Node32& operator=(const Node32&) = delete;

    constexpr inline Node32 clone(std::size_t& alloc) const {
        Node32 result{min_};
        result.min_ = min_;
        result.max_ = max_;

        if (cluster_data_ != nullptr) {
            allocator_t a{alloc};
            result.cluster_data_ = a.allocate(1);
            a.construct(result.cluster_data_, 0, alloc);
            result.cluster_data_->summary = cluster_data_->summary.clone(alloc);
            for (const auto& cluster : cluster_data_->clusters) {
                result.cluster_data_->clusters.emplace(cluster.clone(alloc));
            }
        }
        return result;
    }

    constexpr inline Node32(Node32&& other) noexcept
        : cluster_data_(std::exchange(other.cluster_data_, nullptr))
        , min_(other.min_)
        , max_(other.max_) {
    }
    constexpr inline Node32& operator=(Node32&& other) noexcept {
        if (this != &other) {
            if (cluster_data_ != nullptr) {
                assert(false && "Node32 must be destructed via `.destroy(alloc)` before being assigned to.");
            }
            cluster_data_ = std::exchange(other.cluster_data_, nullptr);
            min_ = other.min_;
            max_ = other.max_;
        }
        return *this;
    }

    // Node32 must be destructed via `.destroy(alloc)`. Failure to do so will result in UB.
    // ~Node32() noexcept = default;

    constexpr inline void destroy(std::size_t& alloc) {
        if (cluster_data_ != nullptr) {
            cluster_data_->summary.destroy(alloc);
            for (const auto& cluster : cluster_data_->clusters) {
                const_cast<subnode_t&>(cluster).destroy(alloc);
            }
            allocator_t a{alloc};
            a.destroy(cluster_data_);
            a.deallocate(cluster_data_, 1);
        }
        cluster_data_ = nullptr;
    }

    static constexpr inline std::size_t universe_size() {
        return std::numeric_limits<index_t>::max();
    }
    constexpr inline index_t min() const {
        return min_;
    }
    constexpr inline index_t max() const {
        return max_;
    }

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

        if (cluster_data_ == nullptr) {
            allocator_t a{alloc};
            cluster_data_ = a.allocate(1);
            a.construct(cluster_data_, h, alloc);
            cluster_data_->clusters.emplace(h, l);
        } else if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            auto& cluster{const_cast<Node16&>(*it)};
            cluster.insert(l, alloc);
        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.emplace(h, l);
        }
    }

    constexpr inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (cluster_data_ == nullptr || cluster_data_->clusters.empty()) {
                return true;
            } else {
                auto min_cluster{cluster_data_->summary.min()};
                auto min_element{cluster_data_->clusters.find(min_cluster)->min()};
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (cluster_data_ == nullptr || cluster_data_->clusters.empty()) {
                max_ = min_;
            } else {
                auto max_cluster{cluster_data_->summary.max()};
                auto max_element{cluster_data_->clusters.find(max_cluster)->max()};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        if (cluster_data_ != nullptr) {
            if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
                if (auto& cluster{const_cast<Node16&>(*it)}; cluster.remove(l, alloc)) {
                    cluster.destroy(alloc);
                    cluster_data_->clusters.erase(it);
                    cluster_data_->summary.remove(h, alloc);
                    if (cluster_data_->clusters.empty()) {
                        destroy(alloc);
                    }
                }
            }
        }

        return false;
    }

    constexpr inline bool contains(index_t x) const {
        if (x == min_ || x == max_) {
            return true;
        }

        if (cluster_data_ == nullptr) return false;

        const auto [h, l] {decompose(x)};
        if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            return it->contains(l);
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

        if (!cluster_data_) {
            return std::make_optional(max_);
        }

        const auto [h, l] {decompose(x)};

        if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end() && l < it->max()) {
            if (auto succ{it->successor(l)}; succ.has_value()) {
                return index(h, *succ);
            }
        }

        if (auto succ_cluster{cluster_data_->summary.successor(h)}; succ_cluster.has_value()) {
            auto min_element{cluster_data_->clusters.find(*succ_cluster)->min()};
            return index(*succ_cluster, min_element);
        }

        return std::make_optional(max_);
    }

    constexpr inline std::optional<index_t> predecessor(index_t x) const {
        if (x > max_) {
            return std::make_optional(max_);
        }
        if (x <= min_) {
            return std::nullopt;
        }

        if (cluster_data_ == nullptr) {
            return std::make_optional(min_);
        }

        const auto [h, l] {decompose(x)};

        if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end() && l > it->min()) {
            if (auto pred{it->predecessor(l)}; pred.has_value()) {
                return index(h, *pred);
            }
        }

        if (auto pred_cluster{cluster_data_->summary.predecessor(h)}; pred_cluster.has_value()) {
            auto max_element{cluster_data_->clusters.find(*pred_cluster)->max()};
            return index(*pred_cluster, max_element);
        }

        return std::make_optional(min_);
    }

    constexpr inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (cluster_data_ == nullptr) {
            return base_count;
        }

        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            cluster_data_->clusters.begin(), cluster_data_->clusters.end(),
            base_count, std::plus<>{}, [](const auto& cluster) { return cluster.size(); }
        );
    }

    constexpr inline VebTreeMemoryStats get_memory_stats() const {
        if (cluster_data_ == nullptr) {
            return VebTreeMemoryStats{0, 0, 1};
        }

        auto stats{cluster_data_->summary.get_memory_stats()};
        stats.total_clusters += cluster_data_->clusters.size();
        stats.total_nodes += 1;

        return std::ranges::fold_left(
            cluster_data_->clusters,
            stats,
            [](auto acc, const auto& cluster) {
                auto stats{cluster.get_memory_stats()};
                acc.total_nodes += stats.total_nodes;
                acc.total_clusters += stats.total_clusters;
                acc.max_depth = std::max(acc.max_depth, stats.max_depth + 1);
                return acc;
            }
        );
    }

private:
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

public:
    constexpr inline bool is_tombstone() const {
        return min_ > max_;
    }

    constexpr inline decltype(auto) or_inplace(this auto&& self, const Node32& other, std::size_t& alloc) {
        self.insert(other.min_, alloc);
        self.insert(other.max_, alloc);

        if (other.cluster_data_ == nullptr) {
            return std::forward<decltype(self)>(self);
        }

        if (self.cluster_data_ == nullptr) {
            allocator_t a{alloc};
            self.cluster_data_ = a.allocate(1);
            a.construct(self.cluster_data_, 0, alloc);
            self.cluster_data_->summary = other.cluster_data_->summary.clone(alloc);
            for (const auto& cluster : other.cluster_data_->clusters) {
                self.cluster_data_->clusters.emplace(cluster.clone(alloc));
            }

            return std::forward<decltype(self)>(self);
        }

        auto& this_summary{self.cluster_data_->summary};
        auto& this_clusters{self.cluster_data_->clusters};
        const auto& other_summary{other.cluster_data_->summary};
        const auto& other_clusters{other.cluster_data_->clusters};
        
        this_summary.or_inplace(other_summary, alloc);
        for (const auto& other_cluster : other_clusters) {
            if (auto it{this_clusters.find(other_cluster)}; it != this_clusters.end()) {
                auto& cluster{const_cast<Node16&>(*it)};
                cluster.or_inplace(other_cluster, alloc);
            } else {
                this_clusters.emplace(other_cluster.clone(alloc));
            }
        }
        return std::forward<decltype(self)>(self);
    }

    constexpr inline decltype(auto) and_inplace(this auto&& self, const Node32& other, std::size_t& alloc) {
        const auto potential_min{std::max(self.min_, other.min_)};
        const auto potential_max{std::min(self.max_, other.max_)};
        auto new_min{self.contains(potential_min) && other.contains(potential_min) ? std::make_optional(potential_min) : std::nullopt};
        auto new_max{self.contains(potential_max) && other.contains(potential_max) ? std::make_optional(potential_max) : std::nullopt};

        if (potential_min >= potential_max || self.cluster_data_ == nullptr || other.cluster_data_ == nullptr) {
            return std::forward<decltype(self)>(self).empty_clusters_or_tombstone(new_min, new_max, alloc);
        }

        auto& this_summary{self.cluster_data_->summary};
        auto& this_clusters{self.cluster_data_->clusters};
        const auto& other_summary{other.cluster_data_->summary};
        const auto& other_clusters{other.cluster_data_->clusters};

        if (this_summary.and_inplace(other_summary, alloc).is_tombstone()) {
            return std::forward<decltype(self)>(self).empty_clusters_or_tombstone(new_min, new_max, alloc);
        }

        for (auto cluster_idx{std::make_optional(this_summary.min())}; cluster_idx.has_value(); cluster_idx = this_summary.successor(*cluster_idx)) {
            if (const auto this_it{this_clusters.find(*cluster_idx)}, other_it{other_clusters.find(*cluster_idx)};
                this_it != this_clusters.end() && other_it != other_clusters.end()) {
                if (auto& cluster{const_cast<Node16&>(*this_it)}; cluster.and_inplace(*other_it, alloc).is_tombstone()) {
                    cluster.destroy(alloc);
                    this_clusters.erase(this_it);
                    if (this_summary.remove(*cluster_idx, alloc)) {
                        return std::forward<decltype(self)>(self).empty_clusters_or_tombstone(new_min, new_max, alloc);
                    }
                }
            }
        }

        self.max_ = new_max.has_value() ? *new_max : index(this_summary.max(), this_clusters.find(this_summary.max())->max());
        self.min_ = new_min.has_value() ? *new_min : index(this_summary.min(), this_clusters.find(this_summary.min())->min());

        if (self.max_ != potential_max) {
            const auto it{this_clusters.find(this_summary.max())};
            auto& cluster{const_cast<Node16&>(*it)};
            cluster.remove(static_cast<subindex_t>(self.max_), alloc) && this_summary.remove(this_summary.max(), alloc);
        }
        if (self.min_ != potential_min && !this_clusters.empty()) {
            const auto it{this_clusters.find(this_summary.min())};
            auto& cluster{const_cast<Node16&>(*it)};
            cluster.remove(static_cast<subindex_t>(self.min_), alloc) && this_summary.remove(this_summary.min(), alloc);
        }

        if (this_clusters.empty()) {
            return std::forward<decltype(self)>(self).empty_clusters_or_tombstone(self.min_, self.max_, alloc);
        }

        return std::forward<decltype(self)>(self);
    }
};

#endif // NODE32_HPP
