#ifndef NODE64_HPP
#define NODE64_HPP

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include <ranges>
#include <numeric>
#include <execution>
#include <functional>
#include "node32.hpp"
#include "VebCommon.hpp"
#include "allocator/tracking_allocator.hpp"

struct Node64 {
    friend struct VebTree;
public:
    using subnode_t = Node32;
    using subindex_t = subnode_t::index_t;
    using index_t = std::uint64_t;

private:
    struct cluster_data_t {
        using key_type = subindex_t;
        using hash_t = std::hash<key_type>;
        using key_equal_t = std::equal_to<key_type>;
        using value_type = std::pair<const key_type, subnode_t>;
        using allocator_t = tracking_allocator<value_type>;
        using cluster_map_t = std::unordered_map<key_type, subnode_t, hash_t, key_equal_t, allocator_t>;

        cluster_map_t clusters;
        subnode_t summary;

        cluster_data_t(subindex_t x, std::size_t& alloc)
            : clusters{allocator_t{alloc}}
            , summary{subnode_t::new_with(x)} {
        }

        auto values() const { return std::views::values(clusters); }
        auto values() { return std::views::values(clusters); }
    };
    using allocator_t = tracking_allocator<cluster_data_t>;

    cluster_data_t* cluster_data_{};
    index_t min_{};
    index_t max_{};

    static constexpr inline std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 32), static_cast<subindex_t>(x)};
    }
    static constexpr inline index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>(high) << 32 | low;
    }

    constexpr inline explicit Node64() = default;

public:
    static constexpr inline Node64 new_with(index_t x) {
        Node64 node{};
        node.min_ = x;
        node.max_ = x;
        return node;
    }

    static constexpr inline Node64 new_from_node32(Node32&& old_storage, std::size_t& alloc) {
        Node64 node{};
        const auto old_min{old_storage.min()};
        const auto old_max{old_storage.max()};
        node.min_ = static_cast<index_t>(old_min);
        node.max_ = static_cast<index_t>(old_max);

        old_storage.remove(old_min, alloc);
        if (old_min != old_max) {
            old_storage.remove(old_max, alloc);
        }

        if (old_storage.size() > 0) {
            allocator_t a{alloc};
            node.cluster_data_ = a.allocate(1);
            a.construct(node.cluster_data_, 0, alloc);
            node.cluster_data_->clusters.emplace(0, std::move(old_storage));
        }
        return node;
    }

    static constexpr inline Node64 new_from_node16(subnode_t::subnode_t&& old_storage, std::size_t& alloc) {
        auto intermediate{Node32::new_from_node16(std::move(old_storage), alloc)};
        return new_from_node32(std::move(intermediate), alloc);
    }
    static constexpr inline Node64 new_from_node8(subnode_t::subnode_t::subnode_t old_storage, std::size_t& alloc) {
        auto intermediate{Node32::new_from_node8(old_storage, alloc)};
        return new_from_node32(std::move(intermediate), alloc);
    }

    // Node64 is non-copyable. Copying would require deep copies of potentially large structures.
    // If you need to make a copy, use `.clone(alloc)` instead.
    Node64(const Node64& other) = delete;
    Node64& operator=(const Node64&) = delete;

    constexpr inline Node64 clone(std::size_t& alloc) const {
        auto result{new_with(min_)};
        result.min_ = min_;
        result.max_ = max_;

        if (cluster_data_ != nullptr) {
            allocator_t a{alloc};
            result.cluster_data_ = a.allocate(1);
            a.construct(result.cluster_data_, 0, alloc);
            result.cluster_data_->summary = cluster_data_->summary.clone(alloc);
            for (const auto& [key, cluster] : cluster_data_->clusters) {
                result.cluster_data_->clusters.emplace(key, cluster.clone(alloc));
            }
        }
        return result;
    }

    constexpr inline Node64(Node64&& other) noexcept
        : cluster_data_(std::exchange(other.cluster_data_, nullptr))
        , min_(other.min_)
        , max_(other.max_) {
    }
    constexpr inline Node64& operator=(Node64&& other) noexcept {
        if (this != &other) {
            if (cluster_data_ != nullptr) {
                assert(false && "Node64 must be destructed via `.destroy(alloc)` before being assigned to.");
            }
            cluster_data_ = std::exchange(other.cluster_data_, nullptr);
            min_ = other.min_;
            max_ = other.max_;
        }
        return *this;
    }

    // Node64 must be destructed via `.destroy(alloc)`. Failure to do so will result in UB.
#ifdef DEBUG
    ~Node64() noexcept {
        assert(cluster_data_ == nullptr && "Node64 must be destructed via `.destroy(alloc)` before going out of scope.");
    }
#else
    // ~Node64() noexcept = default;
#endif

    constexpr inline void destroy(std::size_t& alloc) {
        if (cluster_data_) {
            cluster_data_->summary.destroy(alloc);
            for (auto& [key, cluster] : cluster_data_->clusters) {
                cluster.destroy(alloc);
            }
            allocator_t a{alloc};
            a.destroy(cluster_data_);
            a.deallocate(cluster_data_, 1);
            cluster_data_ = nullptr;
        }
    }

    static constexpr inline std::uint64_t universe_size() {
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
            cluster_data_->clusters.emplace(h, subnode_t::new_with(l));
        } else if (const auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            it->second.insert(l, alloc);
        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.emplace(h, subnode_t::new_with(l));
        }
    }

    constexpr inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (cluster_data_ == nullptr) {
                return true;
            } else {
                const auto min_cluster{cluster_data_->summary.min()};
                [[assume(cluster_data_->summary.contains(min_cluster))]];
                const auto& cluster = cluster_data_->clusters.at(min_cluster);
                const auto min_element{cluster.min()};
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (cluster_data_ == nullptr) {
                max_ = min_;
            } else {
                const auto max_cluster{cluster_data_->summary.max()};
                [[assume(cluster_data_->summary.contains(max_cluster))]];
                const auto& cluster = cluster_data_->clusters.at(max_cluster);
                const auto max_element{cluster.max()};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        if (cluster_data_ != nullptr) {
            if (const auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
                if (it->second.remove(l, alloc)) {
                    it->second.destroy(alloc);
                    cluster_data_->clusters.erase(it);
                    if (cluster_data_->summary.remove(h, alloc)) {
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

        if (cluster_data_ == nullptr) {
            return false;
        }

        const auto [h, l] {decompose(x)};
        if (const auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            return it->second.contains(l);
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

        if (!cluster_data_) {
            return std::make_optional(max_);
        }

        const auto [h, l] {decompose(x)};

        if (const auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end() && l < it->second.max()) {
            if (const auto succ{it->second.successor(l)}; succ.has_value()) {
                return std::make_optional(index(h, *succ));
            }
        }

        if (const auto succ{cluster_data_->summary.successor(h)}; succ.has_value()) {
            const auto l{cluster_data_->clusters.at(*succ).min()};
            return std::make_optional(index(*succ, l));
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

        if (const auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end() && l > it->second.min()) {
            if (const auto pred{it->second.predecessor(l)}; pred.has_value()) {
                return index(h, *pred);
            }
        }

        if (const auto pred{cluster_data_->summary.predecessor(h)}; pred.has_value()) {
            const auto l{cluster_data_->clusters.at(*pred).max()};
            return index(*pred, l);
        }

        return std::make_optional(min_);
    }

    constexpr inline std::size_t size() const {
        const auto base_count{(min_ == max_) ? 1uz : 2uz};

        if (cluster_data_ == nullptr) {
            return base_count;
        }

        return std::transform_reduce(
#ifdef __cpp_lib_execution
            std::execution::par_unseq,
#endif
            cluster_data_->values().begin(), cluster_data_->values().end(),
            base_count, std::plus<>(), [](const auto& cluster) { return cluster.size(); }
        );
    }

    // helper struct for count_range. allows passing either arg optionally
    struct count_range_args {
        index_t lo{static_cast<index_t>(0)};
        index_t hi{static_cast<index_t>(universe_size())};
    };
    constexpr inline std::size_t count_range(count_range_args args) const {
        const auto [lo, hi] {args};
        auto total{static_cast<std::size_t>(
            (lo <= min_ && min_ <= hi) + (max_ != min_ && lo <= max_ && max_ <= hi)
        )};

        if (cluster_data_ == nullptr) {
            return total;
        }

        const auto& summary{cluster_data_->summary};
        const auto& clusters{cluster_data_->clusters};

        const auto [lcl, lidx] {decompose(lo)};
        const auto [hcl, hidx] {decompose(hi)};
        if (lcl == hcl) {
            if (const auto it{clusters.find(lcl)}; it != clusters.end()) {
                total += it->second.count_range({ .lo = lidx, .hi = hidx });
            }
            return total;
        }

        if (const auto it{clusters.find(lcl)}; it != clusters.end()) {
            total += it->second.count_range({ .lo = lidx });
        }
        if (const auto it{clusters.find(hcl)}; it != clusters.end()) {
            total += it->second.count_range({ .hi = hidx });
        }

        for (auto i{summary.successor(lcl)}; i.has_value() && i.value() < hcl; i = summary.successor(i.value())) {
            total += clusters.find(i.value())->second.size();
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
            cluster_data_->values(),
            stats,
            [](auto acc, const auto& cluster) {
                const auto stats{cluster.get_memory_stats()};
                acc.total_nodes += stats.total_nodes;
                acc.total_clusters += stats.total_clusters;
                acc.max_depth = std::max(acc.max_depth, stats.max_depth + 1);
                return acc;
            }
        );
    }

    // Serialization (Node64 record)
    // Format: min(u64 LE), max(u64 LE), clusters_len(u32 LE)
    // If clusters_len > 0 then follows: summary (Node32 record) then `clusters_len` entries of (cluster_key u32 LE + cluster_node record)
    inline void serialize_payload(std::string &out) const {
        write_u64(out, min_);
        write_u64(out, max_);

        if (cluster_data_ == nullptr) {
            write_u64(out, 0);
            return;
        }

        write_u64(out, static_cast<std::uint64_t>(cluster_data_->clusters.size()));

        cluster_data_->summary.serialize_payload(out);

        for (auto idx = std::make_optional(cluster_data_->summary.min()); idx.has_value(); idx = cluster_data_->summary.successor(idx.value())) {
            [[assume(cluster_data_->summary.contains(idx.value()))]];
            cluster_data_->clusters.find(idx.value())->second.serialize_payload(out);
        }
    }

    static inline Node64 deserialize_from_payload(std::string_view buf, size_t &pos, std::size_t &alloc) {
        Node64 node{};
        node.min_ = read_u64(buf, pos);
        node.max_ = read_u64(buf, pos);

        const auto len = read_u64(buf, pos);
        if (len == 0) {
            return node;
        }

        allocator_t a{alloc};
        node.cluster_data_ = a.allocate(1);
        a.construct(node.cluster_data_, 0, alloc);
        node.cluster_data_->summary = std::move(Node32::deserialize_from_payload(buf, pos, alloc));

        node.cluster_data_->clusters.reserve(len);
        auto key{std::make_optional(node.cluster_data_->summary.min())};
        for (std::size_t i = 0; i < len; ++i) {
            auto cluster = Node32::deserialize_from_payload(buf, pos, alloc);
            [[assume(key.has_value())]];
            node.cluster_data_->clusters.emplace(key.value(), std::move(cluster));
            key = node.cluster_data_->summary.successor(key.value());
        }

        return node;
    }

    constexpr inline bool or_inplace(const Node64& other, std::size_t& alloc) {
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
            for (const auto& [key, cluster] : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(key, cluster.clone(alloc));
            }

            return false;
        }

        auto& s_summary{cluster_data_->summary};
        auto& s_clusters{cluster_data_->clusters};
        const auto& o_summary{other.cluster_data_->summary};
        const auto& o_clusters{other.cluster_data_->clusters};

        s_summary.or_inplace(o_summary, alloc);
        for (const auto& [idx, o_cluster] : o_clusters) {
            if (const auto it{s_clusters.find(idx)}; it != s_clusters.end()) {
                it->second.or_inplace(o_cluster, alloc);
            } else {
                s_clusters.emplace(idx, o_cluster.clone(alloc));
            }
        }
        return false;
    }

    constexpr inline bool and_inplace(const Node64& other, std::size_t& alloc) {
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
        if (s_summary.and_inplace(o_summary, alloc)) {
            return update_minmax();
        }

        // iterate only clusters surviving the summary intersection
        for (auto s_it{s_clusters.begin()}; s_it != s_clusters.end(); ) {
            auto& [key, cluster] = *s_it;
            [[assume(!s_summary.contains(key) || o_summary.contains(key))]];
            if (!s_summary.contains(key) || cluster.and_inplace(o_clusters.at(key), alloc)) {
                cluster.destroy(alloc);
                s_it = s_clusters.erase(s_it);
                if (s_summary.remove(key, alloc)) {
                    return update_minmax();
                }
            } else {
                ++s_it;
            }
        }

        const auto sum_max = s_summary.max();
        [[assume(s_summary.contains(sum_max))]];
        auto& c_max = s_clusters.at(sum_max);
        const auto sum_min = s_summary.min();
        [[assume(s_summary.contains(sum_min))]];
        auto& c_min = s_clusters.at(sum_min);

        max_ = new_max.has_value() ? new_max.value() : index(sum_max, c_max.max());
        min_ = new_min.has_value() ? new_min.value() : index(sum_min, c_min.min());

        if (max_ != s_max && c_max.remove(static_cast<subindex_t>(max_), alloc)) {
            c_max.destroy(alloc);
            s_clusters.erase(sum_max);
            if (s_summary.remove(sum_max, alloc)) {
                destroy(alloc);
                return false;
            }
        }
        if (min_ != s_min && c_min.remove(static_cast<subindex_t>(min_), alloc)) {
            c_min.destroy(alloc);
            s_clusters.erase(sum_min);
            if (s_summary.remove(sum_min, alloc)) {
                destroy(alloc);
                return false;
            }
        }

        return false;
    }

    constexpr inline bool xor_inplace(const Node64& other, std::size_t& alloc) {
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
            for (const auto& [key, cluster] : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(key, cluster.clone(alloc));
            }
        } else {
            auto& s_summary{cluster_data_->summary};
            auto& s_clusters{cluster_data_->clusters};
            const auto& o_clusters{other.cluster_data_->clusters};

            for (const auto& [key, o_cluster] : o_clusters) {
                if (auto it{s_clusters.find(key)}; it != s_clusters.end()) {
                    if (it->second.xor_inplace(o_cluster, alloc)) {
                        it->second.destroy(alloc);
                        s_clusters.erase(it);
                        // don't destroy early here, as other clusters might still be created
                        s_summary.remove(key, alloc);
                    }
                } else {
                    s_summary.insert(key, alloc);
                    s_clusters.emplace(key, o_cluster.clone(alloc));
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

#endif // NODE64_HPP
