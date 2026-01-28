#ifndef NODE16_HPP
#define NODE16_HPP

#include <cassert>
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
 * The total size of this struct is 16 bytes on 64-bit systems, ie two registers. As such, we should prefer passing
 *   instances of this struct by value whenever possible.
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
struct Node16 {
    friend struct VebTree;
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
        return static_cast<index_t>((high << 8) | low);
    }

    constexpr inline subnode_t* find(subindex_t x) {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }
    constexpr inline const subnode_t* find(subindex_t x) const {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }

    constexpr inline void grow_capacity_if_needed(std::size_t& alloc) {
        const auto size{cluster_data_->size()};
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
            cluster_data_->clusters_, size, new_data->clusters_
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


        const auto idx{cluster_data_->index_of(hi)};
        if (cluster_data_->summary_.contains(hi)) {
            cluster_data_->clusters_[idx].insert(lo);
            return;
        }

        grow_capacity_if_needed(alloc);

        if (const auto size{cluster_data_->size()}; idx < size) {
            const auto begin{cluster_data_->clusters_ + idx};
            const auto end{cluster_data_->clusters_ + size};
            std::move_backward(begin, end, end + 1);
        }
        cluster_data_->clusters_[idx] = subnode_t{lo};
        cluster_data_->summary_.insert(hi);
    }

public:
    constexpr inline explicit Node16(index_t hi, index_t lo)
        : cluster_data_{nullptr}, capacity_{0}, key_{hi}, min_{lo}, max_{lo} {
    }

    constexpr inline Node16(subnode_t old_storage, std::size_t& alloc)
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
            if (cluster_data_ != nullptr) {
                assert(false && "Node16 must be destructed via `.destroy()` before being assigned to.");
            }
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
        emplace(h, l, alloc);
    }

    constexpr inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (cluster_data_ == nullptr) {
                if (max_ == min_) {
                    return true;
                } else {
                    min_ = max_;
                    return false;
                }
            } else {
                const auto min_cluster{cluster_data_->summary_.min()};
                const auto min_element{cluster_data_->clusters_[0].min()};
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (cluster_data_ == nullptr) {
                if (max_ == min_) {
                    return true;
                } else {
                    max_ = min_;
                    return false;
                }
            } else {
                const auto max_cluster{cluster_data_->summary_.max()};
                const auto idx_max = cluster_data_->index_of(max_cluster);
                [[assume(idx_max < cluster_data_->size())]];
                const auto max_element{cluster_data_->clusters_[idx_max].max()};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        if (auto* cluster{find(h)}; cluster != nullptr && cluster->remove(l)) {
            const auto idx{cluster_data_->index_of(h)};
            const auto size{cluster_data_->size()};
            const auto begin{cluster_data_->clusters_ + idx + 1};
            const auto end{cluster_data_->clusters_ + size};
            std::move(begin, end, begin - 1);

            if (cluster_data_->summary_.remove(h)) {
                destroy(alloc);
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
            return std::make_optional(min_);
        }
        if (x >= max_) {
            return std::nullopt;
        }

        if (cluster_data_ == nullptr) {
            return std::make_optional(max_);
        }

        const auto [h, l] {decompose(x)};

        if (const auto* cluster{find(h)}; cluster != nullptr && l < cluster->max()) {
            if (auto succ{cluster->successor(l)}; succ.has_value()) {
                return std::make_optional(index(h, *succ));
            }
        }

        if (auto succ_cluster{cluster_data_->summary_.successor(h)}; succ_cluster.has_value()) {
            const auto idx = cluster_data_->index_of(*succ_cluster);
            [[assume(idx < cluster_data_->size())]];
            const auto min_element{cluster_data_->clusters_[idx].min()};
            return std::make_optional(index(*succ_cluster, min_element));
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

        if (const auto* cluster{find(h)}; cluster != nullptr && l > cluster->min()) {
            if (auto pred{cluster->predecessor(l)}; pred.has_value()) {
                return std::make_optional(index(h, *pred));
            }
        }

        if (auto pred_cluster{cluster_data_->summary_.predecessor(h)}; pred_cluster.has_value()) {
            const auto idx = cluster_data_->index_of(*pred_cluster);
            [[assume(idx < cluster_data_->size())]];
            const auto max_element{cluster_data_->clusters_[idx].max()};
            return std::make_optional(index(*pred_cluster, max_element));
        }

        return std::make_optional(min_);
    }

    constexpr inline std::size_t size() const {
        const auto base_count{(min_ == max_) ? 1uz : 2uz};
        if (cluster_data_ == nullptr) {
            return base_count;
        }

        const auto* const data{cluster_data_->clusters_};
        const auto count{cluster_data_->size()};
        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            data, data + count, base_count, std::plus<>{},
            [](const auto& cluster) { return cluster.size(); }
        );
    }

    constexpr inline std::uint16_t key() const {
        return key_;
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

    constexpr inline bool or_inplace(const Node16& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (other.cluster_data_ == nullptr) {
            return false;
        }

        allocator_t a{alloc};
        if (cluster_data_ == nullptr) {
            const auto size{other.cluster_data_->size()};
            cluster_data_ = reinterpret_cast<cluster_data_t*>(a.allocate(size + 1));
            cluster_data_->summary_ = other.cluster_data_->summary_;
            std::copy_n(
#ifdef __cpp_lib_execution
                std::execution::unseq,
#endif
                other.cluster_data_->clusters_, size, cluster_data_->clusters_
            );
            capacity_ = static_cast<std::uint16_t>(size);

            return false;
        }

        auto& s_summary{cluster_data_->summary_};
        auto* s_clusters{cluster_data_->clusters_};
        const auto& o_summary{other.cluster_data_->summary_};
        const auto* o_clusters{other.cluster_data_->clusters_};

        // pre compute merged summary.
        // if the size is the same as self's, we can do everything in place since we won't need to add any new clusters
        // this probably isn't the common case, but it's worth optimizing for nonetheless to avoid unnecessary allocations and copies
        auto merge_summary{s_summary};
        merge_summary.or_inplace(o_summary);
        const auto new_capacity{merge_summary.size()};
        if (new_capacity == cluster_data_->size()) {
            std::size_t i{};
            std::size_t j{};
            for (auto idx{std::make_optional(s_summary.min())}; idx.has_value(); idx = s_summary.successor(*idx)) {
                // just because the merge summary contains this index doesn't mean other does
                // we need to find it in other using idx to validate that j is the correct cluster
                // regardless, we always advance i since self definitely contains this cluster
                if (o_summary.contains(*idx)) {
                    s_clusters[i].or_inplace(o_clusters[j]);
                    ++j;
                }
                ++i;
            }
            return false;
        }

        const auto new_cluster_data{reinterpret_cast<cluster_data_t*>(a.allocate(new_capacity + 1))};
        const auto& new_summary{new_cluster_data->summary_ = merge_summary};
        auto* new_clusters{new_cluster_data->clusters_};

        std::size_t i{};
        std::size_t j{};
        std::size_t k{};
        for (auto idx{std::make_optional(new_summary.min())}; idx.has_value(); idx = new_summary.successor(*idx)) {
            // here we know that at least one of self or other contains idx, but we don't know which
            // we need to check both summaries and advance only the corresponding cluster iterators
            const bool in_s = s_summary.contains(*idx);
            const bool in_o = o_summary.contains(*idx);
            if (in_s && in_o) {
                s_clusters[i].or_inplace(o_clusters[j++]);
                new_clusters[k++] = s_clusters[i++];
            } else if (in_s) {
                new_clusters[k++] = s_clusters[i++];
            } else if (in_o) {
                new_clusters[k++] = o_clusters[j++];
            } else {
                std::unreachable();
            }
        }
        destroy(alloc);
        cluster_data_ = new_cluster_data;
        capacity_ = static_cast<std::uint16_t>(new_capacity);
        return false;
    }

    constexpr inline bool and_inplace(const Node16& other, std::size_t& alloc) {
        const auto s_min{min_};
        const auto s_max{max_};
        const auto i_min{std::max(s_min, other.min_)};
        const auto i_max{std::min(s_max, other.max_)};
        auto new_min{contains(i_min) && other.contains(i_min) ? std::make_optional(i_min) : std::nullopt};
        auto new_max{contains(i_max) && other.contains(i_max) ? std::make_optional(i_max) : std::nullopt};

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

        // if this is not the correct min, we will need to update it during iteration
        bool min_out{!new_min.has_value()};
        if (!min_out) {
            min_ = new_min.value();
        }

        auto& s_summary{cluster_data_->summary_};
        auto* s_clusters{cluster_data_->clusters_};
        const auto& o_summary{other.cluster_data_->summary_};
        const auto* o_clusters{other.cluster_data_->clusters_};

        // reduce future work by pre computing summary intersection. if empty, we are done
        // makes iterating clusters easier as we only need to consider clusters that we know will survive intersection
        // unfortunately, we need to clone s_summary here because iterating below requires preserving the original state to find correct indices
        // cloning a node8 is cheap enough though
        auto int_summary{s_summary};
        if (int_summary.and_inplace(o_summary)) {
            return update_minmax();
        }
        // iterate only clusters surviving the summary intersection
        // writeback inplace from the start of the array, overwriting removed clusters
        // avoids allocation of a new array
        std::size_t i{};
        for (auto int_hi_o{std::make_optional(int_summary.min())}; int_hi_o.has_value(); int_hi_o = int_summary.successor(*int_hi_o)) {
            const auto int_hi{int_hi_o.value()};
            // unconditionally access these clusters here. we know they must both exist. otherwise int_hi would not be set in the summary intersection
            const auto s_i{cluster_data_->index_of(int_hi)};
            const auto o_i{other.cluster_data_->index_of(int_hi)};

            auto& int_cluster{s_clusters[s_i]};
            // first check if the cluster intersection is empty
            if (int_cluster.and_inplace(o_clusters[o_i])) {
                if (int_summary.remove(int_hi)) {
                    // this early exit isn't real. it can only happen on the last int_hi_o. doesn't save us from iterating.
                    return update_minmax();
                }
                // cluster is empty, skip min check and writeback
                continue;
            }
            // cluster is non-empty. check if we need to update min_. only happens once, at i == 0.
            // which might not end up being the true 0'th cluster if it gets removed here
            if (min_out) {
                min_out = false;
                min_ = index(int_hi, int_cluster.min());
                // update new_min if we exit with the above update_minmax. we want to ensure min_ is kept correct there
                new_min = std::make_optional(min_);
                if (int_cluster.remove(static_cast<subindex_t>(min_))) {
                    if (int_summary.remove(int_hi)) {
                        // node is now clusterless, but not empty since min_ at least exists.
                        // update max_ and exit
                        destroy(alloc);
                        max_ = new_max.has_value() ? new_max.value() : min_;
                        return false;
                    }
                    // cluster is empty, skip writeback
                    continue;
                }
            }
            // writeback cluster to its new position, if gaps were created by removed clusters
            // might be quicker to unconditionally writeback, but this avoids unnecessary writes in the case of highly overlapping clusters
            if (s_i != i++) {
                s_clusters[i - 1] = int_cluster;
            }
        }

        max_ = new_max.has_value() ? new_max.value() : index(int_summary.max(), s_clusters[i - 1].max());
        if (max_ != s_max && s_clusters[i - 1].remove(static_cast<subindex_t>(max_)) && int_summary.remove(int_summary.max())) {
            // node is now clusterless, but not empty since min_ and max_ exist.
            destroy(alloc);
            return false;
        }
        
        // now that we're done iterating, we can finally update the summary to the intersection
        s_summary = int_summary;
        return false;
    }

    constexpr inline bool xor_inplace(const Node16& other, std::size_t& alloc) {
        const auto s_min{min_};
        const auto s_max{max_};
        const auto o_min{other.min_};
        const auto o_max{other.max_};

        // ensure that self contains the true edges of both nodes
        // push min_ and max_ down to the clusters if needed
        if (o_min < s_min) {
            insert(o_min, alloc);
        }
        if (o_max > s_max) {
            insert(o_max, alloc);
        }
        // don't remove s_min/max yet if it was equal to o_min, since that would pull another value up
        // leading to recursively checking for removal of values until we reach values distinct from other


        allocator_t a{alloc};
        if (other.cluster_data_ == nullptr) {
            // Only need to adjust min and max
        } else if (cluster_data_ == nullptr) {
            const std::size_t size = other.cluster_data_->size();
            cluster_data_ = reinterpret_cast<cluster_data_t*>(a.allocate(size + 1));
            cluster_data_->summary_ = other.cluster_data_->summary_;
            std::copy_n(
#ifdef __cpp_lib_execution
                std::execution::unseq,
#endif
                other.cluster_data_->clusters_, size, cluster_data_->clusters_
            );
            capacity_ = static_cast<std::uint16_t>(size);
        } else {
            auto& s_summary{cluster_data_->summary_};
            auto* s_clusters{cluster_data_->clusters_};
            const auto& o_summary{other.cluster_data_->summary_};
            const auto* o_clusters{other.cluster_data_->clusters_};

            // pre compute merged summary.
            // if the size is the same as self's, we can do everything in place. since at most, we will be removing clusters
            // this probably isn't the common case, but it's worth optimizing for nonetheless to avoid unnecessary allocations and copies
            auto union_summary{s_summary};
            union_summary.or_inplace(o_summary);
            auto max_size = union_summary.size();
            if (max_size == s_summary.size()) {
                std::size_t i{};
                std::size_t j{};
                std::size_t k{};
                for (auto idx{std::make_optional(s_summary.min())}; idx.has_value(); idx = s_summary.successor(*idx)) {
                    // just because the merge summary contains this index doesn't mean other does
                    // we need to find it in other using idx to validate that j is the correct cluster
                    // regardless, we always advance i since self definitely contains this cluster
                    if (auto& xor_cluster{s_clusters[i++]}; o_summary.contains(*idx)) {
                        // these removals should probably be batched (andnot) to avoid traversing the summary multiple times
                        // that would require us to track which clusters are being removed and only remove them after the loop
                        // in node16 this is less needful because removing from a node8 is free
                        if (xor_cluster.xor_inplace(o_clusters[j++]) && s_summary.remove(*idx)) {
                            // this early exit isn't real. it can only happen on the last idx. it doesn't save us from iterating.
                            destroy(alloc);
                            break;
                        } else {
                            s_clusters[k++] = xor_cluster;
                        }
                    } else {
                        s_clusters[k++] = xor_cluster;
                    }
                }
            } else {
                auto* new_data{reinterpret_cast<cluster_data_t*>(a.allocate(max_size + 1))};
                auto& new_summary{new_data->summary_ = std::move(union_summary)};
                auto* new_clusters{new_data->clusters_};

                std::size_t i{};
                std::size_t j{};
                std::size_t k{};
                for (auto idx{std::make_optional(new_summary.min())}; idx.has_value(); idx = new_summary.successor(*idx)) {
                    const bool in_s = s_summary.contains(*idx);
                    const bool in_o = o_summary.contains(*idx);
                    if (in_s && in_o) {
                        if (auto& xor_cluster{s_clusters[i++]}; xor_cluster.xor_inplace(o_clusters[j++])) {
                            if (new_summary.remove(*idx)) {
                                a.deallocate(reinterpret_cast<subnode_t*>(new_data), max_size + 1);
                                new_data = nullptr;
                                max_size = 0;
                                break;
                            }
                        } else {
                            new_clusters[k++] = xor_cluster;
                        }
                    } else if (in_s) {
                        new_clusters[k++] = s_clusters[i++];
                    } else if (in_o) {
                        new_clusters[k++] = o_clusters[j++];
                    } else {
                        std::unreachable();
                    }
                }
                destroy(alloc);
                cluster_data_ = new_data;
                capacity_ = static_cast<std::uint16_t>(max_size);
            }
        }

        // self must contain o_min if it was less than s_min due to the insert at the top of the function
        // we handle the case where o_min == s_min below this handles the case where o_min > s_min
        // we do not need to check the return value here, as removing o_min cannot empty this node as s_min must exist
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
        // other contains s_min either if it was equal to o_min or in one of other's clusters.
        // if it was equal to o_min, we pull up the next minimum from the clusters, which by now we know is not in other
        // if it was in one of the clusters, that cluster will still exist in self, so we need to remove it from the cluster
        return (other.contains(s_min) && remove(s_min, alloc)) ||
               (other.contains(s_max) && remove(s_max, alloc));
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
