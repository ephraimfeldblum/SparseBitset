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
              summary(0, x) {}
    };
    cluster_data_t* cluster_data_{};
    
    index_t min_{};
    index_t max_{};

    static constexpr std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 16), static_cast<subindex_t>(x)};
    }
    static constexpr index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 16 | low;
    }

public:
    inline explicit Node32(index_t x)
        : min_(x), max_(x) {
    }

    inline Node32(Node16&& old_storage, std::size_t& alloc)
        : cluster_data_{nullptr}
        , min_(old_storage.min())
        , max_(old_storage.max())
    {
        auto old_min{static_cast<subindex_t>(min_)};
        auto old_max{static_cast<subindex_t>(max_)};

        old_storage.remove(old_min, alloc);
        if (old_min != old_max) {
            old_storage.remove(old_max, alloc);
        }

        if (old_storage.size() > 0) {
            auto data_alloc{tracking_allocator<cluster_data_t>{alloc}};
            auto* ptr = data_alloc.allocate(1);
            cluster_data_ = new (ptr) cluster_data_t(0, alloc);
            cluster_data_->clusters.emplace(std::move(old_storage));
        }
    }

    void destroy(std::size_t& alloc) {
        if (cluster_data_) {
            cluster_data_->summary.destroy(alloc);
            for (const auto& cluster : cluster_data_->clusters) {
                const_cast<subnode_t&>(cluster).destroy(alloc);
            }
            tracking_allocator<cluster_data_t> a(alloc);
            std::destroy_at(cluster_data_);
            a.deallocate(cluster_data_, 1);
            cluster_data_ = nullptr;
        }
    }

    Node32 clone(std::size_t& alloc) const {
        Node32 result(min_);
        result.min_ = min_;
        result.max_ = max_;

        if (cluster_data_) {
            auto data_alloc{tracking_allocator<cluster_data_t>{alloc}};
            auto* ptr = data_alloc.allocate(1);
            result.cluster_data_ = new (ptr) cluster_data_t(0, alloc);
            result.cluster_data_->summary = cluster_data_->summary.clone(alloc);
            for (const auto& cluster : cluster_data_->clusters) {
                result.cluster_data_->clusters.emplace(cluster.clone(alloc));
            }
        }
        return result;
    }

    Node32(Node32&& other) noexcept
        : cluster_data_(std::exchange(other.cluster_data_, nullptr))
        , min_(other.min_)
        , max_(other.max_) {
    }
    Node32& operator=(Node32&& other) noexcept {
        if (this != &other) {
            cluster_data_ = std::exchange(other.cluster_data_, nullptr);
            min_ = other.min_;
            max_ = other.max_;
        }
        return *this;
    }

    static constexpr std::size_t universe_size() { return std::numeric_limits<index_t>::max(); }
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

        if (cluster_data_ == nullptr) {
            auto data_alloc{tracking_allocator<cluster_data_t>{alloc}};
            auto* ptr = data_alloc.allocate(1);
            cluster_data_ = new (ptr) cluster_data_t(h, alloc);
            cluster_data_->clusters.emplace(h, l);
        } else if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            auto& cluster{const_cast<Node16&>(*it)};
            cluster.insert(l, alloc);
        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.emplace(h, l);
        }
    }

    inline bool remove(index_t x, std::size_t& alloc) {
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

        const auto [h, l] = decompose(x);

        if (cluster_data_ != nullptr) {
            if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
                auto& cluster{const_cast<Node16&>(*it)};
                if (cluster.remove(l, alloc)) {
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

    inline bool contains(index_t x) const {
        if (x == min_ || x == max_) {
            return true;
        }

        if (cluster_data_ == nullptr) return false;

        const auto [h, l] = decompose(x);
        if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
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

    inline std::optional<index_t> predecessor(index_t x) const {
        if (x > max_) {
            return max_;
        }
        if (x <= min_) {
            return std::nullopt;
        }

        if (cluster_data_ == nullptr) return min_;

        const auto [h, l] = decompose(x);

        if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end() && l > it->min()) {
            if (auto pred{it->predecessor(l)}; pred.has_value()) {
                return index(h, *pred);
            }
        }

        if (auto pred_cluster{cluster_data_->summary.predecessor(h)}; pred_cluster.has_value()) {
            auto max_element{cluster_data_->clusters.find(*pred_cluster)->max()};
            return index(*pred_cluster, max_element);
        }

        return min_;
    }

    inline std::size_t size() const {
        std::size_t base_count = (min_ == max_) ? 1uz : 2uz;

        if (cluster_data_ == nullptr) return base_count;

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
                auto stats{cluster_data_->summary.get_memory_stats()};
                stats.total_clusters += cluster_data_->clusters.size();
                stats.total_nodes += 1;
                return stats;
            }(),
            [](VebTreeMemoryStats acc, const auto& cluster) {
                auto cluster_stats{cluster.get_memory_stats()};
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

        if (other.cluster_data_ == nullptr) {
            return *this;
        }

        if (cluster_data_ == nullptr) {
            auto data_alloc{tracking_allocator<cluster_data_t>(alloc)};
            auto* ptr = data_alloc.allocate(1);
            cluster_data_ = new (ptr) cluster_data_t(0, alloc);
            cluster_data_->summary = other.cluster_data_->summary.clone(alloc);
            for (const auto& cluster : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(cluster.clone(alloc));
            }

            return *this;
        }

        cluster_data_->summary.or_inplace(other.cluster_data_->summary, alloc);
        for (const auto& other_cluster : other.cluster_data_->clusters) {
            if (auto it{cluster_data_->clusters.find(other_cluster)}; it != cluster_data_->clusters.end()) {
                auto& cluster{const_cast<Node16&>(*it)};
                cluster.or_inplace(other_cluster, alloc);
            } else {
                cluster_data_->clusters.emplace(other_cluster.clone(alloc));
            }
        }
        return *this;
    }

private:
    Node32& empty_clusters_or_tombstone(std::optional<index_t> new_min, std::optional<index_t> new_max, std::size_t& alloc) {
        destroy(alloc);
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
        auto new_min{contains(potential_min) && other.contains(potential_min) ? std::make_optional(potential_min) : std::nullopt};
        auto new_max{contains(potential_max) && other.contains(potential_max) ? std::make_optional(potential_max) : std::nullopt};
        if (potential_min >= potential_max || !cluster_data_ || !other.cluster_data_) {
            return empty_clusters_or_tombstone(new_min, new_max, alloc);
        }

        if (cluster_data_->summary.and_inplace(other.cluster_data_->summary, alloc).is_tombstone()) {
            return empty_clusters_or_tombstone(new_min, new_max, alloc);
        }

        for (auto cluster_idx{std::make_optional(cluster_data_->summary.min())}; cluster_idx.has_value(); cluster_idx = cluster_data_->summary.successor(*cluster_idx)) {
            if (auto this_it{cluster_data_->clusters.find(*cluster_idx)}, other_it{other.cluster_data_->clusters.find(*cluster_idx)};
                this_it != cluster_data_->clusters.end() && other_it != other.cluster_data_->clusters.end()) {
                if (auto& cluster{const_cast<Node16&>(*this_it)};
                    cluster.and_inplace(*other_it, alloc).is_tombstone() && (cluster_data_->clusters.erase(this_it), cluster_data_->summary.remove(*cluster_idx, alloc))) {
                    return empty_clusters_or_tombstone(new_min, new_max, alloc);
                }
            }
        }

        min_ = *new_min.or_else([&] {
            auto min_cluster{cluster_data_->summary.min()};
            auto min_element{cluster_data_->clusters.find(min_cluster)->min()};
            return std::make_optional(index(min_cluster, min_element));
        });
        max_ = *new_max.or_else([&] {
            auto max_cluster{cluster_data_->summary.max()};
            auto max_element{cluster_data_->clusters.find(max_cluster)->max()};
            return std::make_optional(index(max_cluster, max_element));
        });

        if (max_ != potential_max) {
            auto it{cluster_data_->clusters.find(cluster_data_->summary.max())};
            auto& cluster = const_cast<Node16&>(*it);
            cluster.remove(static_cast<subindex_t>(max_), alloc) && cluster_data_->summary.remove(cluster_data_->summary.max(), alloc);
        }
        if (min_ != potential_min) {
            auto it{cluster_data_->clusters.find(cluster_data_->summary.min())};
            auto& cluster = const_cast<Node16&>(*it);
            cluster.remove(static_cast<subindex_t>(min_), alloc) && cluster_data_->summary.remove(cluster_data_->summary.min(), alloc);
        }

        if (cluster_data_->clusters.empty()) {
            return empty_clusters_or_tombstone(min_, max_, alloc);
        }

        return *this;
    }
};

#endif // NODE32_HPP
