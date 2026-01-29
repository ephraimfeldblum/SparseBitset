#ifndef NODE32_HPP
#define NODE32_HPP

#include <cassert>
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
 * The total size of this struct is 16 bytes on 64-bit systems, ie two registers. As such, we should prefer passing
 *   instances of this struct by value whenever possible.
 */
struct Node32 {
    friend struct VebTree;
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

    constexpr inline Node32(subnode_t&& old_storage, std::size_t& alloc)
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
            auto& cluster{const_cast<subnode_t&>(*it)};
            cluster.insert(l, alloc);
        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.emplace(h, l);
        }
    }

    constexpr inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (cluster_data_ == nullptr) {
                return true;
            } else {
                const auto min_cluster{cluster_data_->summary.min()};
                [[assume(cluster_data_->summary.contains(min_cluster))]];
                const auto it_min = cluster_data_->clusters.find(min_cluster);
                [[assume(it_min != cluster_data_->clusters.end())]];
                const auto min_element{it_min->min()};
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (cluster_data_ == nullptr) {
                max_ = min_;
            } else {
                const auto max_cluster{cluster_data_->summary.max()};
                [[assume(cluster_data_->summary.contains(max_cluster))]];
                const auto it_max = cluster_data_->clusters.find(max_cluster);
                [[assume(it_max != cluster_data_->clusters.end())]];
                const auto max_element{it_max->max()};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        if (cluster_data_ != nullptr) {
            if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
                if (auto& cluster{const_cast<subnode_t&>(*it)}; cluster.remove(l, alloc)) {
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
            [[assume(cluster_data_->summary.contains(*succ_cluster))]];
            const auto it = cluster_data_->clusters.find(*succ_cluster);
            [[assume(it != cluster_data_->clusters.end())]];
            const auto min_element{it->min()};
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
            [[assume(cluster_data_->summary.contains(*pred_cluster))]];
            const auto it = cluster_data_->clusters.find(*pred_cluster);
            [[assume(it != cluster_data_->clusters.end())]];
            const auto max_element{it->max()};
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
            std::execution::par_unseq,
#endif
            cluster_data_->clusters.begin(), cluster_data_->clusters.end(),
            base_count, std::plus<>{}, [](const auto& cluster) { return cluster.size(); }
        );
    }

    constexpr inline std::size_t count_range(index_t lo, index_t hi) const {
        auto total{static_cast<std::size_t>(
            (lo <= min_ && min_ <= hi) + (max_ != min_ && lo <= max_ && max_ <= hi)
        )};

        if (cluster_data_ == nullptr) {
            return total;
        }

        const auto& summary{cluster_data_->summary};
        const auto& clusters{cluster_data_->clusters};

        const auto [lo_cl, lo_low] {decompose(lo)};
        const auto [hi_cl, hi_low] {decompose(hi)};
        if (lo_cl == hi_cl) {
            if (const auto it{clusters.find(lo_cl)}; it != clusters.end()) {
                total += it->count_range(lo_low, hi_low);
            }
            return total;
        }

        if (summary.contains(lo_cl)) {
            total += clusters.find(lo_cl)->count_range(lo_low, static_cast<subindex_t>(subnode_t::universe_size()));
        }
        if (summary.contains(hi_cl)) {
            total += clusters.find(hi_cl)->count_range(static_cast<subindex_t>(0), hi_low);
        }

        for (auto i{summary.successor(lo_cl).value()}; i < hi_cl; i = summary.successor(i).value()) {
            total += clusters.find(i)->size();
        }

        return total;
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

    constexpr inline bool or_inplace(const Node32& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (other.cluster_data_ == nullptr) {
            return false;
        }

        if (cluster_data_ == nullptr) {
            allocator_t a{alloc};
            cluster_data_ = a.allocate(1);
            a.construct(cluster_data_, 0, alloc);
            cluster_data_->summary = other.cluster_data_->summary.clone(alloc);
            for (const auto& cluster : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(cluster.clone(alloc));
            }

            return false;
        }

        auto& s_summary{cluster_data_->summary};
        auto& s_clusters{cluster_data_->clusters};
        const auto& o_summary{other.cluster_data_->summary};
        const auto& o_clusters{other.cluster_data_->clusters};
        
        s_summary.or_inplace(o_summary, alloc);
        for (const auto& o_cluster : o_clusters) {
            if (auto it{s_clusters.find(o_cluster)}; it != s_clusters.end()) {
                const_cast<subnode_t&>(*it).or_inplace(o_cluster, alloc);
            } else {
                s_clusters.emplace(o_cluster.clone(alloc));
            }
        }
        return false;
    }

    constexpr inline bool and_inplace(const Node32& other, std::size_t& alloc) {
        const auto s_min{min_};
        const auto s_max{max_};
        const auto i_min{std::max(s_min, other.min_)};
        const auto i_max{std::min(s_max, other.max_)};
        const auto new_min{contains(i_min) && other.contains(i_min) ? std::make_optional(i_min) : std::nullopt};
        const auto new_max{contains(i_max) && other.contains(i_max) ? std::make_optional(i_max) : std::nullopt};
    
        const auto update_minmax = [&] {
            destroy(alloc);
            if (new_min.has_value() && new_max.has_value()) {
                min_ = new_min.value();
                max_ = new_max.value();
                return false;
            }
            if (new_min.has_value()) {
                min_ = max_ = new_min.value();
                return false;
            }
            if (new_max.has_value()) {
                min_ = max_ = new_max.value();
                return false;
            }
            return true;
        };

        // easy outs: no overlap aside from mins/maxs, or either node has no clusters
        if (i_min >= i_max || cluster_data_ == nullptr || other.cluster_data_ == nullptr) {
            return update_minmax();
        }

        auto& s_summary{cluster_data_->summary};
        auto& s_clusters{cluster_data_->clusters};
        const auto& o_summary{other.cluster_data_->summary};
        const auto& o_clusters{other.cluster_data_->clusters};

        // pre compute summary intersection. if empty, we are done
        // makes iterating clusters easier as we only need to consider clusters in s_summary
        // avoids cloning the summary unnecessarily
        if (s_summary.and_inplace(o_summary, alloc)) {
            return update_minmax();
        }

        // iterate only clusters surviving the summary intersection
        for (auto it{s_clusters.begin()}; it != s_clusters.end(); ) {
            // if the summary no longer contains this cluster, it was removed during the intersection
            auto& cluster{const_cast<subnode_t&>(*it)};
            const auto key{cluster.key()};
            if (auto o_it = o_clusters.find(key); !s_summary.contains(key) || cluster.and_inplace(*o_it, alloc)) {
                cluster.destroy(alloc);
                it = s_clusters.erase(it);
                if (s_summary.remove(key, alloc)) {
                    // early exit here. s_clusters still might contain nodes that will be removed unconditionally in destroy.
                    return update_minmax();
                }
            } else {
                ++it;
            }
        }

        const auto sum_max = s_summary.max();
        [[assume(s_summary.contains(sum_max))]];
        const auto it_max = s_clusters.find(sum_max);
        [[assume(it_max != s_clusters.end())]];
        auto& c_max = const_cast<subnode_t&>(*it_max);
        const auto sum_min = s_summary.min();
        [[assume(s_summary.contains(sum_min))]];
        const auto it_min = s_clusters.find(sum_min);
        [[assume(it_min != s_clusters.end())]];
        auto& c_min = const_cast<subnode_t&>(*it_min);

        max_ = new_max.has_value() ? new_max.value() : index(sum_max, c_max.max());
        min_ = new_min.has_value() ? new_min.value() : index(sum_min, c_min.min());

        if (max_ != s_max && c_max.remove(static_cast<subindex_t>(max_), alloc)) {
            c_max.destroy(alloc);
            s_clusters.erase(it_max);
            if (s_summary.remove(sum_max, alloc)) {
                destroy(alloc);
                return false;
            }
        }
        if (min_ != s_min && c_min.remove(static_cast<subindex_t>(min_), alloc)) {
            c_min.destroy(alloc);
            s_clusters.erase(it_min);
            if (s_summary.remove(sum_min, alloc)) {
                destroy(alloc);
                return false;
            }
        }

        return false;
    }

    constexpr inline bool xor_inplace(const Node32& other, std::size_t& alloc) {
        const auto s_min{min_};
        const auto s_max{max_};
        const auto o_min{other.min_};
        const auto o_max{other.max_};

        if (o_min < s_min) {
            insert(o_min, alloc);
        }
        if (o_max > s_max) {
            insert(o_max, alloc);
        }

        allocator_t a{alloc};
        if (other.cluster_data_ == nullptr) {
        } else if (cluster_data_ == nullptr) {
            cluster_data_ = a.allocate(1);
            a.construct(cluster_data_, 0, alloc);
            cluster_data_->summary = other.cluster_data_->summary.clone(alloc);
            for (const auto& cluster : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(cluster.clone(alloc));
            }
        } else {
            auto& s_summary{cluster_data_->summary};
            auto& s_clusters{cluster_data_->clusters};
            const auto& o_clusters{other.cluster_data_->clusters};

            for (const auto& o_cluster : o_clusters) {
                if (auto it{s_clusters.find(o_cluster.key())}; it != s_clusters.end()) {
                    auto& s_cluster{const_cast<subnode_t&>(*it)};
                    if (s_cluster.xor_inplace(o_cluster, alloc)) {
                        s_cluster.destroy(alloc);
                        s_clusters.erase(it);
                        // don't destroy early here, as other clusters might still be created
                        s_summary.remove(o_cluster.key(), alloc);
                    }
                } else {
                    s_summary.insert(o_cluster.key(), alloc);
                    s_clusters.emplace(o_cluster.clone(alloc));
                }
            }

            // now that all xors are done, check if we need to destroy the cluster_data
            if (s_clusters.empty()) {
                destroy(alloc);
            }
        }

        if (s_min < o_min) {
            if (contains(o_min)) {
                remove(o_min, alloc);
            } else {
                insert(o_min, alloc);
            }
        }
        if (s_max > o_max) {
            if (contains(o_max)) {
                remove(o_max, alloc);
            } else {
                insert(o_max, alloc);
            }
        }

        return (other.contains(s_min) && remove(s_min, alloc)) || 
               (other.contains(s_max) && remove(s_max, alloc));
    }
};

#endif // NODE32_HPP
