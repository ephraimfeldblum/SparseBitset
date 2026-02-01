#ifndef NODE16_HPP
#define NODE16_HPP

#include <algorithm>  // std::copy_n, std::move, std::move_backward, std::min, std::max
#include <cassert>    // assert
#include <cstddef>    // std::size_t
#include <cstdint>    // std::uint16_t, std::uint64_t
#include <execution>  // std::execution::unseq
#include <functional> // std::plus
#include <numeric>    // std::transform_reduce
#include <optional>   // std::optional, std::nullopt
#include <utility>    // std::move, std::forward, std::pair, std::exchange

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
 *   - A subindex `cap_` field to track the allocated size of the clusters array.
 *   - A subindex `len_` field to track the number of used clusters.
 * 
 * The total size of this struct is 16 bytes on 64-bit systems, ie two registers.
 * The purpose of this design is to optimize memory usage while maintaining fast operations on the underlying nodes.
 * The `cluster_data_t` structure is allocated dynamically to allow for flexible sizing of the clusters array.
 * The `cap_` and `len_` fields help manage the dynamic array of clusters, allowing for efficient resizing when necessary.
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
        subnode_t unfilled_;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        subnode_t clusters_[];
#pragma GCC diagnostic pop

        // resident mask = summary_ & unfilled_
        constexpr inline subnode_t resident_mask() const {
            auto r = summary_;
            r.and_inplace(unfilled_);
            return r;
        }

        // Returns the index of where cluster `x` would be located in `clusters_`
        // If `x` is not present, this is the index where it would be inserted. Uses the
        // resident mask so indices refer to the packed resident clusters array.
        constexpr inline subindex_t index_of(subindex_t x) const {
            if (x == 0) {
                return 0;
            }
            const auto resident = resident_mask();
            return static_cast<subindex_t>(resident.count_range({ .hi = static_cast<subindex_t>(x - 1) }));
        }
        constexpr inline subnode_t* find(subindex_t x) {
            return (summary_.contains(x) && unfilled_.contains(x)) ? &clusters_[index_of(x)] : nullptr;
        }
        constexpr inline const subnode_t* find(subindex_t x) const {
            return (summary_.contains(x) && unfilled_.contains(x)) ? &clusters_[index_of(x)] : nullptr;
        }
        constexpr inline std::size_t size() const {
            return summary_.size();
        }
        constexpr inline std::size_t resident_count() const {
            return resident_mask().size();
        }

        // range is inclusive: [lo, hi] to handle 256-element array counts correctly
        // This counts raw bits across the packed resident clusters starting at `lo`/`hi` indices.
        constexpr inline std::size_t count(subindex_t lo, subindex_t hi) const {
            const auto* const data{reinterpret_cast<const std::uint64_t*>(clusters_ + lo)};
            const auto num_words{(hi + 1 - lo) * sizeof *clusters_ / sizeof *data};

            std::size_t accum1{};
            std::size_t accum2{};
            std::size_t accum3{};
            std::size_t accum4{};
            for (auto i{0uz}; i < num_words; i += 4) {
                accum1 += std::popcount(data[i + 0]);
                accum2 += std::popcount(data[i + 1]);
                accum3 += std::popcount(data[i + 2]);
                accum4 += std::popcount(data[i + 3]);
            }
            return accum1 + accum2 + accum3 + accum4;
        }
    };

    index_t key_{};
    index_t min_{};
    index_t max_{};
    // `cap_` and `len_` are stored as `subindex_t` to save space.
    // - cluster_data_ == nullptr => 0,
    // - 1..255 => 1..255,
    // - 0 => 256,
    subindex_t cap_{};
    subindex_t len_{};
    cluster_data_t* cluster_data_{};

    static constexpr inline std::pair<subindex_t, subindex_t> decompose(index_t x) {
        return {static_cast<subindex_t>(x >> 8), static_cast<subindex_t>(x)};
    }
    static constexpr inline index_t index(subindex_t high, subindex_t low) {
        return static_cast<index_t>((high << 8) | low);
    }

    // Allocate a new cluster_data_t with space for `cap` clusters.
    // If `other` is provided, copy its summary and clusters up to `other_size` (if non-zero)
    // otherwise fall back to `other->size()`.
    static constexpr inline cluster_data_t* create(std::size_t& alloc, std::size_t cap, const cluster_data_t* other, std::size_t other_size) {
        allocator_t a{alloc};
        auto* data = reinterpret_cast<cluster_data_t*>(a.allocate(cap + 2));
        if (other != nullptr) {
            data->summary_ = other->summary_;
            data->unfilled_ = other->unfilled_;
            const auto copy_count = std::min(cap, other_size);
            std::copy_n(
#ifdef __cpp_lib_execution
                std::execution::unseq,
#endif
                other->clusters_, copy_count, data->clusters_
            );
        } else {
            data->unfilled_ = subnode_t::new_all();
        }
        return data;
    }
    static constexpr inline cluster_data_t* create(std::size_t& alloc, std::size_t cap, subnode_t summary) {
        auto* data = create(alloc, cap, nullptr, 0);
        data->summary_ = summary;
        return data;
    }
    static constexpr inline cluster_data_t* create(std::size_t& alloc, std::size_t cap, subindex_t hi, subnode_t lo) {
        auto* data = create(alloc, cap, nullptr, 0);
        data->summary_ = subnode_t::new_with(hi);
        data->unfilled_.insert(hi);
        data->clusters_[0] = lo;
        return data;
    }

    constexpr inline subnode_t* find(subindex_t x) {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }
    constexpr inline const subnode_t* find(subindex_t x) const {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }

    constexpr inline void grow(std::size_t& alloc) {
        const auto len{get_len()};
        const auto cur_cap = get_cap();
        if (len < cur_cap) {
            return;
        }
        const auto new_cap{std::min(256uz, cur_cap + (cur_cap / 4) + 1)};
        auto* new_data = create(alloc, new_cap, cluster_data_, len);
        destroy(alloc);
        cluster_data_ = new_data;
        set_cap(new_cap);
        set_len(len);
    }

    constexpr inline void emplace(subindex_t hi, subindex_t lo, std::size_t& alloc) {
        if (cluster_data_ == nullptr) {
            cluster_data_ = create(alloc, 1, hi, subnode_t::new_with(lo));
            set_cap(1);
            set_len(1);
            return;
        }

        const auto idx{cluster_data_->index_of(hi)};
        if (cluster_data_->summary_.contains(hi)) {
            // if the cluster is implicitly filled, it's already present logically
            if (!cluster_data_->unfilled_.contains(hi)) {
                return;
            }
            cluster_data_->clusters_[idx].insert(lo);
            // if the node becomes full, remove the resident node and mark it implicitly-filled
            if (cluster_data_->clusters_[idx].size() == 256) {
                const auto size{get_len()};
                const auto begin{cluster_data_->clusters_ + idx + 1};
                const auto end{cluster_data_->clusters_ + size};
                std::move(begin, end, begin - 1);
                set_len(size - 1);
                cluster_data_->unfilled_.remove(hi);
            }
            return;
        }

        grow(alloc);

        const auto len{get_len()};
        if (idx < len) {
            const auto begin{cluster_data_->clusters_ + idx};
            const auto end{cluster_data_->clusters_ + len};
            std::move_backward(begin, end, end + 1);
        }
        cluster_data_->clusters_[idx] = subnode_t::new_with(lo);
        cluster_data_->summary_.insert(hi);
        cluster_data_->unfilled_.insert(hi);
        set_len(len + 1);
    }

    constexpr inline std::size_t get_cap() const {
        if (cluster_data_ == nullptr) {
            return 0;
        }
        return cap_ == 0 ? 256uz : static_cast<std::size_t>(cap_);
    }
    constexpr inline void set_cap(std::size_t c) {
        cap_ = static_cast<subindex_t>(c);
    }

    constexpr inline std::size_t get_len() const {
        if (cluster_data_ == nullptr) {
            return 0;
        }
        if (len_ == 0) {
            // len_ == 0 is overloaded to mean 256, but if we have zero resident clusters
            // (all clusters are implicitly-filled), we need to return 0 instead of 256.
            return cluster_data_->resident_count() == 0 ? 0uz : 256uz;
        }
        return static_cast<std::size_t>(len_);
    }
    constexpr inline void set_len(std::size_t s) {
        len_ = static_cast<subindex_t>(s);
    }

    constexpr inline explicit Node16() = default;

public:
    static constexpr inline Node16 new_with(index_t hi, index_t lo) {
        Node16 node{};
        node.key_ = static_cast<std::uint16_t>(hi);
        node.min_ = static_cast<index_t>(lo);
        node.max_ = static_cast<index_t>(lo);
        return node;
    }

    static constexpr inline Node16 new_from_node8(subnode_t old_storage, std::size_t& alloc) {
        Node16 node{};
        const auto old_min{old_storage.min()};
        const auto old_max{old_storage.max()};

        node.min_ = static_cast<index_t>(old_min);
        node.max_ = static_cast<index_t>(old_max);

        old_storage.remove(old_min);
        if (old_min != old_max) {
            old_storage.remove(old_max);
        }

        if (old_storage.size() > 0) {
            node.cluster_data_ = create(alloc, 1, 0, old_storage);
            node.set_cap(1);
            node.set_len(1);
        }
        return node;
    }

    constexpr inline void destroy(std::size_t& alloc) {
        if (cluster_data_ != nullptr) {
            allocator_t a{alloc};
            a.deallocate(reinterpret_cast<subnode_t*>(cluster_data_), get_cap() + 2);
            cluster_data_ = nullptr;
            set_cap(0);
            set_len(0);
        }
    }

    constexpr inline Node16 clone(std::size_t& alloc) const {
        auto result{new_with(key_, min_)};
        result.max_ = max_;

        if (cluster_data_ != nullptr) {
            const auto len{get_len()};
            result.cluster_data_ = create(alloc, len, cluster_data_, len);
            result.set_cap(len);
            result.set_len(len);
        }
        return result;
    }

    constexpr inline Node16(Node16&& other) noexcept
        : key_{other.key_}
        , min_{other.min_}
        , max_{other.max_}
        , cap_{std::exchange(other.cap_, 0)}
        , len_{std::exchange(other.len_, 0)}
        , cluster_data_{std::exchange(other.cluster_data_, nullptr)} {
    }

    constexpr inline Node16& operator=(Node16&& other) noexcept {
        if (this != &other) {
            if (cluster_data_ != nullptr) {
                assert(false && "Node16 must be destructed via `.destroy()` before being assigned to.");
            }
            cluster_data_ = std::exchange(other.cluster_data_, nullptr);
            cap_ = std::exchange(other.cap_, 0);
            len_ = std::exchange(other.len_, 0);
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
#ifdef DEBUG
    ~Node16() noexcept {
        assert(cluster_data_ == nullptr && "Node16 must be destructed via `.destroy()` before going out of scope.");
    }
#else
    // ~Node16() noexcept = default;
#endif

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
                const auto min_element{cluster_data_->unfilled_.contains(min_cluster) ?
                    cluster_data_->clusters_[cluster_data_->index_of(min_cluster)].min() : static_cast<subindex_t>(0)};
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
                const auto max_element{cluster_data_->unfilled_.contains(max_cluster) ?
                    cluster_data_->clusters_[cluster_data_->index_of(max_cluster)].max() : static_cast<subindex_t>(255)};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        // If cluster exists implicitly (filled) we need to materialize it as all-but-l and mark it resident
        if (cluster_data_->summary_.contains(h) && !cluster_data_->unfilled_.contains(h)) {
            // materialize an all-but-l node
            auto node = subnode_t::new_all_but(l);
            grow(alloc);
            const auto idx{cluster_data_->index_of(h)};
            const auto size{get_len()};
            if (idx < size) {
                const auto begin{cluster_data_->clusters_ + idx};
                const auto end{cluster_data_->clusters_ + size};
                std::move_backward(begin, end, end + 1);
            }
            cluster_data_->clusters_[idx] = node;
            cluster_data_->unfilled_.insert(h);
            set_len(size + 1);
            return false;
        }

        if (auto* cluster{find(h)}; cluster != nullptr && cluster->remove(l)) {
            const auto idx{cluster_data_->index_of(h)};
            const auto size{get_len()};
            const auto begin{cluster_data_->clusters_ + idx + 1};
            const auto end{cluster_data_->clusters_ + size};
            std::move(begin, end, begin - 1);
            set_len(size - 1);

            if (cluster_data_->summary_.remove(h)) {
                // ensure unfilled_ treats non-existent clusters as set
                cluster_data_->unfilled_.insert(h);
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
        if (cluster_data_ == nullptr) {
            return false;
        }
        if (!cluster_data_->summary_.contains(h)) {
            return false;
        }
        if (!cluster_data_->unfilled_.contains(h)) {
            return true; // implicitly filled cluster
        }
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

        if (cluster_data_->summary_.contains(h)) {
            if (!cluster_data_->unfilled_.contains(h)) {
                // implicitly filled cluster: min=0, max=255
                if (l < static_cast<subindex_t>(255)) {
                    return std::make_optional(index(h, static_cast<subindex_t>(l + 1)));
                }
            } else if (const auto* cluster{find(h)}; cluster != nullptr && l < cluster->max()) {
                if (auto succ{cluster->successor(l)}; succ.has_value()) {
                    return std::make_optional(index(h, *succ));
                }
            }
        }

        if (auto succ_cluster{cluster_data_->summary_.successor(h)}; succ_cluster.has_value()) {
            if (!cluster_data_->unfilled_.contains(*succ_cluster)) {
                return std::make_optional(index(*succ_cluster, static_cast<subindex_t>(0)));
            }
            const auto idx = cluster_data_->index_of(*succ_cluster);
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

        if (cluster_data_->summary_.contains(h)) {
            if (!cluster_data_->unfilled_.contains(h)) {
                // implicitly filled cluster: min=0, max=255
                if (l > static_cast<subindex_t>(0)) {
                    return std::make_optional(index(h, static_cast<subindex_t>(l - 1)));
                }
            } else if (const auto* cluster{find(h)}; cluster != nullptr && l > cluster->min()) {
                if (auto pred{cluster->predecessor(l)}; pred.has_value()) {
                    return std::make_optional(index(h, *pred));
                }
            }
        }

        if (auto pred_cluster{cluster_data_->summary_.predecessor(h)}; pred_cluster.has_value()) {
            if (!cluster_data_->unfilled_.contains(*pred_cluster)) {
                return std::make_optional(index(*pred_cluster, static_cast<subindex_t>(255)));
            }
            const auto idx = cluster_data_->index_of(*pred_cluster);
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
        const auto resident_count = cluster_data_->resident_count();
        const auto resident_bits = resident_count == 0 ? 0 : cluster_data_->count(static_cast<subindex_t>(0), static_cast<subindex_t>(get_len() - 1));
        const auto implicit_clusters = cluster_data_->summary_.size() - resident_count;
        return base_count + resident_bits + implicit_clusters * static_cast<std::size_t>(256);
    }

    // helper struct for count_range. allows passing either arg optionally
    struct count_range_args {
        index_t lo{static_cast<index_t>(0)};
        index_t hi{static_cast<index_t>(universe_size())};
    };
    constexpr inline std::size_t count_range(count_range_args args) const {
        const auto [lo, hi] {args};
        auto acc{static_cast<std::size_t>(
            (lo <= min_ && min_ <= hi) + (max_ != min_ && lo <= max_ && max_ <= hi)
        )};

        if (cluster_data_ == nullptr) {
            return acc;
        }

        const auto [lcl, lidx] {decompose(lo)};
        const auto [hcl, hidx] {decompose(hi)};
        if (lcl == hcl) {
            if (cluster_data_->summary_.contains(lcl)) {
                if (!cluster_data_->unfilled_.contains(lcl)) {
                    acc += static_cast<std::size_t>(hidx - lidx + 1);
                } else if (const auto* cluster{find(lcl)}; cluster != nullptr) {
                    acc += cluster->count_range({ .lo = lidx, .hi = hidx});
                }
            }
            return acc;
        }

        // left cluster partial
        if (cluster_data_->summary_.contains(lcl)) {
            if (!cluster_data_->unfilled_.contains(lcl)) {
                acc += static_cast<std::size_t>(256 - lidx);
            } else if (const auto* cluster{find(lcl)}; cluster != nullptr) {
                acc += cluster->count_range({ .lo = lidx });
            }
        }

        // right cluster partial
        if (cluster_data_->summary_.contains(hcl)) {
            if (!cluster_data_->unfilled_.contains(hcl)) {
                acc += static_cast<std::size_t>(hidx + 1);
            } else if (const auto* cluster{find(hcl)}; cluster != nullptr) {
                acc += cluster->count_range({ .hi = hidx });
            }
        }

        // fully covered internal clusters: iterate summary between lcl and hcl
        for (auto ci{cluster_data_->summary_.successor(lcl)}; ci.has_value() && *ci < hcl; ci = cluster_data_->summary_.successor(*ci)) {
            if (!cluster_data_->unfilled_.contains(*ci)) {
                acc += static_cast<std::size_t>(256);
            } else {
                const auto idx = cluster_data_->index_of(*ci);
                acc += cluster_data_->clusters_[idx].size();
            }
        }

        return acc;
    }

    constexpr inline std::uint16_t key() const {
        return key_;
    }
    constexpr inline void set_key(std::uint16_t new_key) {
        key_ = new_key;
    }

    constexpr inline VebTreeMemoryStats get_memory_stats() const {
        if (cluster_data_ == nullptr) {
            return {0, 0, 1};
        }

        const auto cluster_count{get_len()};
        auto stats{cluster_data_->summary_.get_memory_stats()};
        stats.total_nodes += 1;
        stats.max_depth += 1;
        stats.total_clusters += cluster_count;

        for (std::size_t i{}; i < cluster_count; ++i) {
            const auto cluster_stats{cluster_data_->clusters_[i].get_memory_stats()};
            stats.total_nodes += cluster_stats.total_nodes;
            stats.max_depth = std::max(stats.max_depth, cluster_stats.max_depth + 1);
        }

        return stats;
    }

    inline void serialize_payload(std::string &out) const {
        write_u16(out, min_);
        write_u16(out, max_);

        if (cluster_data_ == nullptr) {
            write_u16(out, 0);
            return;
        }

        // encode resident count + 1 so 0 remains sentinel for no cluster data
        const auto resident_count{cluster_data_->resident_count()};
        write_u16(out, static_cast<std::uint16_t>(resident_count + 1));

        cluster_data_->summary_.serialize_payload(out);
        cluster_data_->unfilled_.serialize_payload(out);

        for (auto idx{0uz}; idx < resident_count; ++idx) {
            cluster_data_->clusters_[idx].serialize_payload(out);
        }
    }

    static inline Node16 deserialize_from_payload(std::string_view buf, std::size_t &pos, std::size_t &alloc) {
        Node16 node{};
        node.min_ = read_u16(buf, pos);
        node.max_ = read_u16(buf, pos);

        const auto raw_len{read_u16(buf, pos)};
        if (raw_len == 0) {
            return node;
        }

        const auto resident_count{static_cast<std::size_t>(raw_len - 1)};

        node.cluster_data_ = create(alloc, resident_count, nullptr, 0);
        node.cluster_data_->summary_ = subnode_t::deserialize_from_payload(buf, pos);
        node.cluster_data_->unfilled_ = subnode_t::deserialize_from_payload(buf, pos);

        for (auto idx{0uz}; idx < resident_count; ++idx) {
            node.cluster_data_->clusters_[idx] = subnode_t::deserialize_from_payload(buf, pos);
        }

        node.set_cap(resident_count);
        node.set_len(resident_count);
        return node;
    }

    constexpr inline bool or_inplace(const Node16& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (other.cluster_data_ == nullptr) {
            return false;
        }

        if (cluster_data_ == nullptr) {
            const auto len{other.get_len()};
            cluster_data_ = create(alloc, len, other.cluster_data_, len);
            set_cap(len);
            set_len(len);

            return false;
        }

        auto& s_summary{cluster_data_->summary_};
        auto* s_clusters{cluster_data_->clusters_};
        auto& s_unfilled{cluster_data_->unfilled_};
        const auto& o_summary{other.cluster_data_->summary_};
        const auto* o_clusters{other.cluster_data_->clusters_};
        const auto& o_unfilled{other.cluster_data_->unfilled_};

        // get a conservative set of clusters that can remain resident, and predict a resident upper bound
        auto merge_summary{s_summary};
        merge_summary.or_inplace(o_summary);
        auto merge_unfilled{s_unfilled};
        merge_unfilled.and_inplace(o_unfilled);
        auto resident{merge_summary};
        resident.and_inplace(merge_unfilled);
        const auto new_size{resident.size()};

        // If predicted upper limit of resident clusters fits in current capacity, use original clusters and do in-place merge
        auto* merge_data = (new_size <= get_cap()) ? cluster_data_ : create(alloc, new_size, merge_summary);
        auto* merge_clusters = merge_data->clusters_;
        merge_data->unfilled_ = merge_unfilled;

        auto i{0uz};
        auto j{0uz};
        auto k{0uz};
        for (auto idx{std::make_optional(resident.min())}; idx.has_value(); idx = resident.successor(*idx)) {
            const auto h = *idx;
            const auto in_s = s_summary.contains(h);
            const auto in_o = o_summary.contains(h);

            if (in_s && in_o) {
                auto tmp = s_clusters[i++];
                tmp.or_inplace(o_clusters[j++]);
                if (tmp.size() == 256) {
                    merge_data->unfilled_.remove(h);
                } else {
                    merge_clusters[k++] = tmp;
                }
            } else if (in_s) {
                merge_clusters[k++] = s_clusters[i++];
            } else if (in_o) {
                merge_clusters[k++] = o_clusters[j++];
            } else {
                std::unreachable();
            }
        }
        if (new_size <= get_cap()) {
            cluster_data_->summary_ = merge_summary;
        } else {
            destroy(alloc);
            cluster_data_ = merge_data;
            set_cap(new_size);
        }
        set_len(k);
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

        // reduce future work by pre computing summary intersection. if empty, we are done
        // makes iterating clusters easier as we only need to consider clusters that we know will survive intersection
        // unfortunately, we need to clone our summary here because iterating below requires preserving the original state to find correct indices
        // cloning a node8 is cheap enough though
        auto int_summary{cluster_data_->summary_};
        if (int_summary.and_inplace(other.cluster_data_->summary_)) {
            return update_minmax();
        }

        // Predict resident clusters: clusters in the intersection that are resident in at least one side
        auto int_unfilled{cluster_data_->unfilled_};
        int_unfilled.or_inplace(other.cluster_data_->unfilled_);
        auto resident{int_summary};
        if (resident.and_inplace(int_unfilled)) {
            // intersection has no resident clusters, only implicit full clusters
            // check if min/max require materialization and exit
            std::optional<subnode_t> min_c_o{std::nullopt};
            std::optional<subnode_t> max_c_o{std::nullopt};
            const auto min_hi{int_summary.min()};
            const auto max_hi{int_summary.max()};
            if (min_out) {
                min_ = index(min_hi, static_cast<subindex_t>(0));
                min_c_o = std::make_optional(subnode_t::new_all_but(0));
                int_unfilled.insert(min_hi);
            }
            if (!new_max.has_value()) {
                max_ = index(max_hi, static_cast<subindex_t>(255));
                if (min_hi == max_hi && min_c_o.has_value()) {
                    // handle min/max collision in same cluster
                    min_c_o.value().remove(static_cast<subindex_t>(255));
                } else {
                    max_c_o = std::make_optional(subnode_t::new_all_but(255));
                }
                int_unfilled.insert(max_hi);
            }
            // either min or max required materialization, might need to create new cluster_data
            const auto new_len{static_cast<std::size_t>(min_c_o.has_value() + max_c_o.has_value())};
            if (new_len > 0) {
                auto* int_data{(new_len <= get_cap()) ? cluster_data_ : create(alloc, new_len, int_summary)};
                int_data->summary_ = int_summary;
                int_data->unfilled_ = int_unfilled;
                auto* int_clusters{int_data->clusters_};
                if (min_c_o.has_value()) {
                    int_clusters[0] = *min_c_o;
                }
                if (max_c_o.has_value()) {
                    int_clusters[new_len - 1] = *max_c_o;
                }
                if (int_data != cluster_data_) {
                    destroy(alloc);
                    cluster_data_ = int_data;
                    set_cap(new_len);
                }
                set_len(new_len);
                return false;
            } else {
                // min/max didn't require materializing new clusters, intersection contains only implicit clusters
                // min must have already been set correctly above in order to reach here
                max_ = new_max.value();
                cluster_data_->summary_ = int_summary;
                cluster_data_->unfilled_ = int_unfilled;
                set_len(0);
                return false;
            }
        }
        // predict if min/max will be removed from non-resident clusters, will we materialize new clusters for them?
        const auto materialize_min{!new_min.has_value() && resident.min() != int_summary.min()};
        const auto materialize_max{!new_max.has_value() && int_summary.min() != int_summary.max() && resident.max() != int_summary.max()};
        const auto resident_count{resident.size() + materialize_min + materialize_max};
        const auto safe_to_inplace{!materialize_min && resident_count <= get_cap()};

        // If predicted resident clusters exceed capacity, allocate a new cluster_data_t and write into it
        auto* int_data = safe_to_inplace ? cluster_data_ : create(alloc, resident_count, int_summary);
        auto* int_clusters = int_data->clusters_;

        auto i{0uz};
        auto j{0uz};
        auto k{0uz};
        for (auto int_hi_o{std::make_optional(int_summary.min())}; int_hi_o.has_value(); int_hi_o = int_summary.successor(*int_hi_o)) {
            const auto int_hi{int_hi_o.value()};
            // both implicit-filled -> result is implicitly-filled (full).
            if (!resident.contains(int_hi)) {
                if (min_out) {
                    min_out = false;
                    min_ = index(int_hi, static_cast<subindex_t>(0));
                    new_min = std::make_optional(min_);
                    int_clusters[k++] = subnode_t::new_all_but(0);
                    int_unfilled.insert(int_hi);
                }
                continue;
            }

            const auto s_resident{cluster_data_->unfilled_.contains(int_hi)};
            const auto o_resident{other.cluster_data_->unfilled_.contains(int_hi)};
            [[assume(s_resident || o_resident)]];

            if (s_resident && o_resident) {
                auto tmp{cluster_data_->clusters_[i++]};
                if (tmp.and_inplace(other.cluster_data_->clusters_[j++])) {
                    if (int_summary.remove(int_hi)) {
                        // last element removed -> update min/max and return
                        if (int_data != cluster_data_) {
                            allocator_t a{alloc};
                            a.deallocate(reinterpret_cast<subnode_t*>(int_data), resident_count + 2);
                        }
                        return update_minmax();
                    }
                    continue;
                }
                int_clusters[k++] = tmp;
            } else if (s_resident) {
                int_clusters[k++] = cluster_data_->clusters_[i++];
            } else if (o_resident) {
                int_clusters[k++] = other.cluster_data_->clusters_[j++];
            } else {
                std::unreachable();
            }

            // cluster is non-empty. check if we need to update min_. only happens once, at the 0'th cluster.
            // which might not end up being the true 0'th cluster if it gets removed here.
            if (min_out) {
                min_out = false;
                min_ = index(int_hi, int_clusters[0].min());
                new_min = std::make_optional(min_);
                if (int_clusters[0].remove(static_cast<subindex_t>(min_))) {
                    // cluster became empty after removing min
                    --k;
                    if (int_summary.remove(int_hi)) {
                        // node is now clusterless, but not empty since min_ at least exists.
                        // update max_ and exit.
                        if (int_data != cluster_data_) {
                            allocator_t a{alloc};
                            a.deallocate(reinterpret_cast<subnode_t*>(int_data), resident_count + 2);
                        }
                        destroy(alloc);
                        max_ = new_max.has_value() ? new_max.value() : min_;
                        return false;
                    }
                }
            }
        }

        if (int_data == cluster_data_) {
            cluster_data_->summary_ = int_summary;
            cluster_data_->unfilled_ = int_unfilled;
        } else {
            destroy(alloc);
            int_data->unfilled_ = int_unfilled;
            cluster_data_ = int_data;
            set_cap(resident_count);
        }

        if (!new_max.has_value()) {
            const auto max_hi{cluster_data_->summary_.max()};
            if (!resident.contains(max_hi)) {
                max_ = index(max_hi, static_cast<subindex_t>(255));
                cluster_data_->unfilled_.insert(max_hi);
                cluster_data_->clusters_[k++] = subnode_t::new_all_but(255);
            } else if (max_ = index(max_hi, cluster_data_->clusters_[k - 1].max()); max_ != s_max && cluster_data_->clusters_[k - 1].remove(static_cast<subindex_t>(max_))) {
                --k;
                if (cluster_data_->summary_.remove(max_hi)) {
                    destroy(alloc);
                    return false;
                }
            }
        }

        set_len(k);
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


        if (other.cluster_data_ == nullptr) {
            // Only need to adjust min and max
        } else if (cluster_data_ == nullptr) {
            const auto len{other.get_len()};
            cluster_data_ = create(alloc, len, other.cluster_data_, len);
            set_cap(len);
            set_len(len);
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
                set_len(k);
            } else {
                auto* new_data = create(alloc, max_size, union_summary);
                auto& new_summary{new_data->summary_};
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
                                allocator_t a{alloc};
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
                set_cap(max_size);
                set_len(k);
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
        constexpr inline bool operator()(const Node16& lhs, const index_t rhs) const {
            return lhs.key_ == rhs;
        }
        constexpr inline bool operator()(const index_t lhs, const Node16& rhs) const {
            return lhs == rhs.key_;
        }
        constexpr inline bool operator()(const index_t lhs, const index_t rhs) const {
            return lhs == rhs;
        }
    };
    struct Hash {
        using is_transparent = void;
        constexpr inline std::size_t operator()(const Node16& node) const {
            return std::hash<index_t>{}(node.key_);
        }
        constexpr inline std::size_t operator()(const index_t key) const {
            return std::hash<index_t>{}(key);
        }
    };
};

#endif // NODE16_HPP
