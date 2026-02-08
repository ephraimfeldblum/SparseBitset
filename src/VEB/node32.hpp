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
 *   - A pointer to a `cluster_data_t` structure containing:
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
            , summary{subnode_t::new_with(0, x)} {
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

    constexpr inline explicit Node32() = default;

public:
    static constexpr inline Node32 new_with(index_t x) {
        Node32 node{};
        node.min_ = x;
        node.max_ = x;
        return node;
    }

    static constexpr inline Node32 new_from_node16(subnode_t&& old_storage, std::size_t& alloc) {
        Node32 node{};
        auto old_min{old_storage.min()};
        auto old_max{old_storage.max()};
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
            node.cluster_data_->clusters.emplace(std::move(old_storage));
        }
        return node;
    }

    static constexpr inline Node32 new_from_node8(subnode_t::subnode_t old_storage, std::size_t& alloc) {
        auto intermediate{Node16::new_from_node8(old_storage, alloc)};
        return new_from_node16(std::move(intermediate), alloc);
    }

    // Node32 is non-copyable. Copying would require deep copies of potentially large structures.
    // If you need to make a copy, use `.clone(alloc)` instead.
    Node32(const Node32& other) = delete;
    Node32& operator=(const Node32&) = delete;

    constexpr inline Node32 clone(std::size_t& alloc) const {
        auto result{new_with(min_)};
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
#ifdef DEBUG
    ~Node32() noexcept {
        assert(cluster_data_ == nullptr && "Node32 must be destructed via `.destroy(alloc)` before going out of scope.");
    }
#else
    // ~Node32() noexcept = default;
#endif

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
        return 1uz + std::numeric_limits<index_t>::max();
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
            cluster_data_->clusters.emplace(subnode_t::new_with(h, l));
        } else if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            auto& cluster{const_cast<subnode_t&>(*it)};
            cluster.insert(l, alloc);
            if (cluster.full()) {
                // convert to implicit representation
                cluster.destroy(alloc);
                cluster_data_->clusters.erase(it);
            }
        } else if (cluster_data_->summary.contains(h)) {
            // cluster is already implicit; nothing to do
            return;

        } else {
            cluster_data_->summary.insert(h, alloc);
            cluster_data_->clusters.emplace(subnode_t::new_with(h, l));
        }
    }

    constexpr inline bool remove(index_t x, std::size_t& alloc) {
        if (x == min_) {
            if (cluster_data_ == nullptr) {
                return true;
            } else {
                const auto min_cluster{cluster_data_->summary.min()};
                const auto it_min{cluster_data_->clusters.find(min_cluster)};
                const auto min_element{it_min == cluster_data_->clusters.end() ? static_cast<subindex_t>(0) : it_min->min()};
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (cluster_data_ == nullptr) {
                max_ = min_;
            } else {
                const auto max_cluster{cluster_data_->summary.max()};
                const auto it_max{cluster_data_->clusters.find(max_cluster)};
                const auto max_element{it_max == cluster_data_->clusters.end() ? static_cast<subindex_t>(subnode_t::universe_size() - 1) : it_max->max()};
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
            } else if (cluster_data_->summary.contains(h)) {
                // cluster is implicitly full; removing `l` means we need to materialize all-but-l
                auto c{subnode_t::new_all_but(l, alloc)};
                c.set_key(h);
                cluster_data_->clusters.emplace(std::move(c));
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
        if (auto it{cluster_data_->clusters.find(h)}; it != cluster_data_->clusters.end()) {
            return it->contains(l);
        }
        // if summary says cluster exists but no resident child present, it is implicit
        return cluster_data_->summary.contains(h);
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
        const auto it{cluster_data_->clusters.find(h)};
        const auto resident{it != cluster_data_->clusters.end()};
        const auto compacted{!resident && cluster_data_->summary.contains(h)};

        // if cluster is resident
        if (resident && l < it->max()) {
            return std::make_optional(index(h, it->successor(l).value()));
        }
        // if cluster is full, next is x+1
        if (compacted && l < static_cast<subindex_t>(subnode_t::universe_size() - 1)) {
            return std::make_optional(x + 1);
        }

        if (auto succ_cluster{cluster_data_->summary.successor(h)}; succ_cluster.has_value()) {
            const auto it{cluster_data_->clusters.find(*succ_cluster)};
            const auto min_element{it != cluster_data_->clusters.end() ? it->min() : static_cast<subindex_t>(0)};
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
        const auto it{cluster_data_->clusters.find(h)};
        const auto resident{it != cluster_data_->clusters.end()};
        const auto compacted{!resident && cluster_data_->summary.contains(h)};

        if (resident && l > it->min()) {
            return std::make_optional(index(h, it->predecessor(l).value()));
        } else if (compacted && l > static_cast<subindex_t>(0)) {
            return std::make_optional(x - 1);
        }

        if (auto pred_cluster{cluster_data_->summary.predecessor(h)}; pred_cluster.has_value()) {
            const auto it{cluster_data_->clusters.find(*pred_cluster)};
            const auto max_element{it != cluster_data_->clusters.end() ? it->max() : static_cast<subindex_t>(subnode_t::universe_size() - 1)};
            return std::make_optional(index(*pred_cluster, max_element));
        }

        return std::make_optional(min_);
    }

    constexpr inline std::size_t size() const {
        auto acc{(min_ == max_) ? 1uz : 2uz};

        if (cluster_data_ == nullptr) {
            return acc;
        }
        acc += (cluster_data_->summary.size() - cluster_data_->clusters.size()) * subnode_t::universe_size();
        return std::transform_reduce(
            cluster_data_->clusters.begin(), cluster_data_->clusters.end(),
            acc, std::plus<>{}, [](const auto& cluster) { return cluster.size(); }
        );
    }

    // helper struct for count_range. allows passing either arg optionally
    struct count_range_args {
        index_t lo{static_cast<index_t>(0)};
        index_t hi{static_cast<index_t>(universe_size() - 1)};
    };
    constexpr inline std::size_t count_range(count_range_args args) const {
        const auto [lo, hi] {args};
        auto acc{static_cast<std::size_t>(
            (lo <= min_ && min_ <= hi) + (max_ != min_ && lo <= max_ && max_ <= hi)
        )};

        if (cluster_data_ == nullptr) {
            return acc;
        }

        const auto& summary{cluster_data_->summary};
        const auto& clusters{cluster_data_->clusters};

        const auto [lcl, lidx] {decompose(lo)};
        const auto [hcl, hidx] {decompose(hi)};
        if (lcl == hcl) {
            if (summary.contains(lcl)) {
                if (const auto it{clusters.find(lcl)}; it != clusters.end()) {
                    acc += it->count_range({ .lo = lidx, .hi = hidx });
                } else {
                    acc += 1uz + hidx - lidx;
                }
            }
            return acc;
        }

        if (summary.contains(lcl)) {
            if (const auto it{clusters.find(lcl)}; it != clusters.end()) {
                acc += it->count_range({ .lo = lidx });
            } else {
                acc += subnode_t::universe_size() - lidx;
            }
        }
        if (summary.contains(hcl)) {
            if (const auto it{clusters.find(hcl)}; it != clusters.end()) {
                acc += it->count_range({ .hi = hidx });
            } else {
                acc += 1uz + hidx;
            }
        }

        for (auto idx{summary.successor(lcl)}; idx.has_value() && idx.value() < hcl; idx = summary.successor(idx.value())) {
            if (const auto it{clusters.find(idx.value())}; it != clusters.end()) {
                acc += it->size();
            } else {
                acc += subnode_t::universe_size();
            }
        }

        return acc;
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

    inline void serialize(std::string &out) const {
        write_u32(out, min_);
        write_u32(out, max_);

        if (cluster_data_ == nullptr) {
            write_u32(out, 0);
            return;
        }
        auto tmp_alloc{0uz};
        auto resident{cluster_data_->summary.clone(tmp_alloc)};
        std::string cl_str{};
        for (auto idx{std::make_optional(cluster_data_->summary.min())}; idx.has_value(); idx = cluster_data_->summary.successor(idx.value())) {
            if (auto it{cluster_data_->clusters.find(idx.value())}; it != cluster_data_->clusters.end()) {
                it->serialize(cl_str);
            } else {
                if (resident.remove(idx.value(), tmp_alloc)) {
                    // all clusters are full. none resident. just write summary and exit.
                    write_u32(out, 1);
                    cluster_data_->summary.serialize(out);
                    resident.destroy(tmp_alloc);
                    return;
                }
            }
        }

        write_u32(out, static_cast<std::uint32_t>(resident.size() + 1)); // +1 because 0 is reserved for empty
        cluster_data_->summary.serialize(out);
        resident.serialize(out);
        resident.destroy(tmp_alloc);
        out.append(cl_str);
    }

    static inline Node32 deserialize(std::string_view buf, size_t &pos, std::size_t &alloc) {
        Node32 node{};
        node.min_ = read_u32(buf, pos);
        node.max_ = read_u32(buf, pos);

        const auto len{read_u32(buf, pos)};
        if (len == 0) {
            return node;
        }

        allocator_t a{alloc};
        node.cluster_data_ = a.allocate(1);
        a.construct(node.cluster_data_, 0, alloc);
        node.cluster_data_->summary = subnode_t::deserialize(buf, pos, alloc);

        const auto cluster_count{len - 1}; // -1 because 0 is reserved for empty
        if (cluster_count == 0) {
            return node;
        }

        auto resident{subnode_t::deserialize(buf, pos, alloc)};
        node.cluster_data_->clusters.reserve(cluster_count);
        for (auto key{std::make_optional(resident.min())}; key.has_value(); key = resident.successor(key.value())) {
            auto cluster{subnode_t::deserialize(buf, pos, alloc)};
            cluster.set_key(key.value());
            node.cluster_data_->clusters.emplace(std::move(cluster));
        }
        resident.destroy(alloc);

        return node;
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
            cluster_data_->clusters.reserve(other.cluster_data_->clusters.size());
            for (const auto& cluster : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(cluster.clone(alloc));
            }

            return false;
        }

        auto& s_summary{cluster_data_->summary};
        auto& s_clusters{cluster_data_->clusters};
        const auto& o_clusters{other.cluster_data_->clusters};

        for (auto o{std::make_optional(other.cluster_data_->summary.min())}; o.has_value(); o = other.cluster_data_->summary.successor(o.value())) {
            const auto key{o.value()};
            if (const auto it{o_clusters.find(key)}; it != o_clusters.end()) {
                const auto& o_cluster{*it};
                if (const auto it{s_clusters.find(key)}; it != s_clusters.end()) {
                    auto& s_cluster{const_cast<subnode_t&>(*it)};
                    s_cluster.or_inplace(o_cluster, alloc);
                    if (s_cluster.full()) {
                        s_cluster.destroy(alloc);
                        s_clusters.erase(it);
                    }
                } else if (!s_summary.contains(key)) {
                    // avoid inserting over an existing implicit cluster
                    s_summary.insert(key, alloc);
                    s_clusters.emplace(o_cluster.clone(alloc));
                }
            } else {
                // cluster is implicitly full in other; ensure it is full in self
                if (!s_summary.contains(key)) {
                    s_summary.insert(key, alloc);
                }
                if (const auto it{s_clusters.find(key)}; it != s_clusters.end()) {
                    // cluster is resident in self but implicitly full in other
                    auto& s_cluster{const_cast<subnode_t&>(*it)};
                    s_cluster.destroy(alloc);
                    s_clusters.erase(it);
                }
            }
        }
        return false;
    }

    constexpr inline bool and_inplace(const Node32& other, std::size_t& alloc) {
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

        auto& s_summary{cluster_data_->summary};
        auto& s_clusters{cluster_data_->clusters};
        const auto& o_summary{other.cluster_data_->summary};
        const auto& o_clusters{other.cluster_data_->clusters};

        // intersect summaries; if empty we're done
        if (s_summary.and_inplace(o_summary, alloc)) {
            return update_minmax();
        }

        // iterate the surviving cluster keys and compute the resulting resident clusters in-place
        // iterate using keys from the summary so we also consider implicit (non-resident) clusters
        for (auto key_opt{std::make_optional(s_summary.min())}; key_opt.has_value(); key_opt = s_summary.successor(key_opt.value())) {
            const auto key{key_opt.value()};
            // find resident clusters on both sides
            const auto s_it = s_clusters.find(key);
            const auto o_it = o_clusters.find(key);

            if (s_it != s_clusters.end()) {
                // resident in self
                auto& s_cluster = const_cast<subnode_t&>(*s_it);
                if (o_it != o_clusters.end()) {
                    // resident in other as well -> intersect in place
                    if (s_cluster.and_inplace(*o_it, alloc)) {
                        s_cluster.destroy(alloc);
                        s_clusters.erase(s_it);
                        if (s_summary.remove(key, alloc)) {
                            return update_minmax();
                        }
                    }
                } else {
                    // other is implicitly full: intersection is unchanged for this resident cluster
                }
            } else {
                if (o_it != o_clusters.end()) {
                    // self was implicitly full but other has a resident cluster -> materialize other's cluster
                    auto clone = o_it->clone(alloc);
                    s_clusters.emplace(std::move(clone));
                } else {
                    // both implicit -> remains implicit
                }
            }
        }

        // if after processing we have no resident clusters, handle the only-implicit case specially
        if (s_clusters.empty()) {
            const auto min_hi{s_summary.min()};
            const auto max_hi{s_summary.max()};

            std::optional<subnode_t> min_c_o{std::nullopt};
            std::optional<subnode_t> max_c_o{std::nullopt};

            if (!new_min.has_value()) {
                // min must be promoted into a concrete element at cluster's start
                min_ = index(min_hi, static_cast<subindex_t>(0));
                min_c_o = std::make_optional(subnode_t::new_all_but(static_cast<subindex_t>(0), alloc));
            }

            if (!new_max.has_value()) {
                // max must be promoted into a concrete element at cluster's end
                max_ = index(max_hi, static_cast<subindex_t>(subnode_t::universe_size() - 1));
                if (min_hi == max_hi && min_c_o.has_value()) {
                    // both edges fall into same cluster, remove the max element from the min cluster
                    min_c_o.value().remove(static_cast<subindex_t>(subnode_t::universe_size() - 1), alloc);
                } else {
                    max_c_o = std::make_optional(subnode_t::new_all_but(static_cast<subindex_t>(subnode_t::universe_size() - 1), alloc));
                }
            }

            const auto new_len = static_cast<std::size_t>(min_c_o.has_value() + max_c_o.has_value());
            if (new_len > 0) {
                if (min_c_o.has_value()) {
                    min_c_o->set_key(min_hi);
                    s_clusters.emplace(std::move(min_c_o.value()));
                }
                if (max_c_o.has_value()) {
                    max_c_o->set_key(max_hi);
                    s_clusters.emplace(std::move(max_c_o.value()));
                }
                return false;
            }

            // no resident clusters required, keep implicit summary and update min/max
            if (new_min.has_value()) {
                min_ = new_min.value();
            }
            if (new_max.has_value()) {
                max_ = new_max.value();
            }
            return false;
        }

        // compute new min/max from surviving summary and resident clusters
        const auto sum_max{s_summary.max()};
        const auto sum_min{s_summary.min()};

        const auto it_max{s_clusters.find(sum_max)};
        const auto it_min{s_clusters.find(sum_min)};

        const bool max_resident{it_max != s_clusters.end()};
        const bool min_resident{it_min != s_clusters.end()};

        max_ = new_max.has_value() ? new_max.value() : index(sum_max, max_resident ? it_max->max() : static_cast<subindex_t>(subnode_t::universe_size() - 1));
        min_ = new_min.has_value() ? new_min.value() : index(sum_min, min_resident ? it_min->min() : static_cast<subindex_t>(0));

        // if min/max were moved into resident clusters, remove elements from those clusters if necessary
        if (!new_max.has_value() && max_resident) {
            auto& c_max = const_cast<subnode_t&>(*it_max);
            if (max_ != s_max && c_max.remove(static_cast<subindex_t>(max_), alloc)) {
                c_max.destroy(alloc);
                s_clusters.erase(it_max);
                if (s_summary.remove(sum_max, alloc)) {
                    destroy(alloc);
                    return false;
                }
            }
        }

        if (!new_min.has_value() && min_resident) {
            auto& c_min = const_cast<subnode_t&>(*it_min);
            if (min_ != s_min && c_min.remove(static_cast<subindex_t>(min_), alloc)) {
                c_min.destroy(alloc);
                s_clusters.erase(it_min);
                if (s_summary.remove(sum_min, alloc)) {
                    destroy(alloc);
                    return false;
                }
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
            cluster_data_->clusters.reserve(other.cluster_data_->clusters.size());
            for (const auto& cluster : other.cluster_data_->clusters) {
                cluster_data_->clusters.emplace(cluster.clone(alloc));
            }
        } else {
            auto& s_summary{cluster_data_->summary};
            auto& s_clusters{cluster_data_->clusters};
            const auto& o_summary{other.cluster_data_->summary};
            const auto& o_clusters{other.cluster_data_->clusters};

            bool summary_empty{false};
            for (auto o{std::make_optional(o_summary.min())}; o.has_value(); o = o_summary.successor(o.value())) {
                const auto key{o.value()};
                if (const auto oit{o_clusters.find(key)}; oit != o_clusters.end()) {
                    const auto& o_cluster{*oit};
                    if (const auto sit{s_clusters.find(key)}; sit != s_clusters.end()) {
                        if (auto& s_cluster{const_cast<subnode_t&>(*sit)}; s_cluster.xor_inplace(o_cluster, alloc)) {
                            s_cluster.destroy(alloc);
                            s_clusters.erase(sit);
                            // don't destroy early here, as other clusters might yet be created
                            if (s_summary.remove(key, alloc)) {
                                summary_empty = true;
                            }
                        } else if (s_cluster.full()) {
                            s_cluster.destroy(alloc);
                            s_clusters.erase(sit);
                        }
                    } else if (summary_empty || !s_summary.contains(key)) {
                        s_summary.insert(key, alloc);
                        s_clusters.emplace(o_cluster.clone(alloc));
                        summary_empty = false;
                    }
                } else if (!summary_empty && s_summary.contains(key)) {
                    if (const auto sit{s_clusters.find(key)}; sit != s_clusters.end()) {
                        auto& s_cluster{const_cast<subnode_t&>(*sit)};
                        s_cluster.not_inplace(alloc);
                    } else {
                        if (s_summary.remove(key, alloc)) {
                            summary_empty = true;
                        }
                    }
                } else {
                    s_summary.insert(key, alloc);
                    summary_empty = false;
                }
            }
            // now that all xors are done, check if we need to destroy cluster_data
            if (summary_empty) {
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
