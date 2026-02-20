#ifndef NODE16_HPP
#define NODE16_HPP

#include <algorithm>  // std::uninitialized_move_n, std::move, std::move_backward, std::min, std::max
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
 *       - An instance of a subnode, which tracks unfilled clusters.
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

    struct Iterator {
        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = index_t;
        using difference_type = std::ptrdiff_t;
        using pointer = void;
        using reference = index_t;
        using subiterator_t = subnode_t::Iterator;

        enum struct State : std::uint8_t {
            Sentinel = 0,
            MinMax = 1,
            Implicit = 2,
            Resident = 3,
        };

        std::uintptr_t tagged_node;
        std::uintptr_t data;

        static constexpr const Node16* canonicalize(std::uintptr_t x) {
            static constexpr std::uintptr_t MASK{0x0000FFFFFFFFFFFF & ~0x3};
            x &= MASK;
            if (x & 0x0000'8000'0000'0000) {
                x |= 0xFFFF'0000'0000'0000;
            }
            return reinterpret_cast<const Node16*>(x);
        }

        static constexpr std::pair<State, const Node16*> decompose_state(std::uintptr_t x) {
            const auto state_tag{static_cast<State>(x & 0x3)};
            return {state_tag, canonicalize(x)};
        }

        static constexpr std::uintptr_t compose(const Node16* node, State state, std::uint8_t idx, std::uint8_t hi_byte) {
            return static_cast<std::uintptr_t>(state)
                | (reinterpret_cast<std::uintptr_t>(node) & 0x0000FFFFFFFFFFFF)
                | (static_cast<std::uintptr_t>(idx) << 48)
                | (static_cast<std::uintptr_t>(hi_byte) << 56);
        }

        static constexpr Iterator create_sentinel(const Node16* node = nullptr) {
            return Iterator{compose(node, State::Sentinel, 0, 0), 0};
        }

        static constexpr Iterator create_min_max(const Node16* node, index_t current) {
            return Iterator{compose(node, State::MinMax, 0, 0), static_cast<std::uintptr_t>(current)};
        }

        static constexpr Iterator create_implicit(const Node16* node, index_t current, subindex_t idx) {
            return Iterator{compose(node, State::Implicit, idx, 0), static_cast<std::uintptr_t>(current)};
        }

        static constexpr Iterator create_resident(const Node16* node, subindex_t hi_byte, subindex_t idx, subiterator_t n8_it) {
            return Iterator{compose(node, State::Resident, idx, hi_byte), n8_it.data};
        }

        constexpr bool is_sentinel() const {
            return get_state() == State::Sentinel;
        }

        constexpr const Node16* get_node_ptr() const {
            return canonicalize(tagged_node);
        }

        constexpr State get_state() const {
            return static_cast<State>(tagged_node & 0x3);
        }

        constexpr subindex_t get_idx() const {
            return static_cast<subindex_t>(tagged_node >> 48);
        }

        constexpr subindex_t get_hi_byte() const {
            return static_cast<subindex_t>(tagged_node >> 56);
        }

        constexpr index_t get_current() const {
            switch (get_state()) {
            case State::Sentinel:
                #ifdef NDEBUG
                std::unreachable();
                #else
                assert(false && "Dereferencing end sentinel");
                #endif
            case State::MinMax: // fallthrough
            case State::Implicit:
                return static_cast<index_t>(data);
            case State::Resident: {
                return index(get_hi_byte(), *subiterator_t{data});
            }
            }
            std::unreachable();
        }

        constexpr reference operator*() const {
            return get_current();
        }

        constexpr Iterator& operator++() {
            const auto node{get_node_ptr()};
            if (node == nullptr) {
                return *this;
            }

            switch (get_state()) {
            case State::Sentinel: {
                *this = node->min();
                return *this;
            }
            case State::MinMax: {
                if (*this == node->max()) {
                    *this = create_sentinel(node);
                } else if (node->cluster_data_ == nullptr) {
                    *this = node->max();
                } else if (const auto hi{*node->cluster_data_->summary_.min()}; node->cluster_data_->unfilled_.contains(hi)) {
                    *this = create_resident(node, hi, 0, node->cluster_data_->clusters_[0].min());
                } else {
                    *this = create_implicit(node, index(hi, 0), 0);
                }
                return *this;
            }
            case State::Implicit: {
                const auto curr{get_current()};
                const auto [hi, lo] {decompose(curr)};

                if (lo < subnode_t::universe_size() - 1) {
                    *this = create_implicit(node, curr + 1, get_idx());
                } else {
                    move_to_successor_cluster(hi);
                }
                return *this;
            }
            case State::Resident: {
                auto n8_it{subiterator_t{data}};
                if (++n8_it != subiterator_t::sentinel()) {
                    *this = create_resident(node, get_hi_byte(), get_idx(), n8_it);
                } else {
                    move_to_successor_cluster(get_hi_byte());
                }
                return *this;
            }
            }
            std::unreachable();
        }

        constexpr Iterator operator++(int) {
            Iterator tmp{*this};
            ++*this;
            return tmp;
        }

        constexpr Iterator& operator--() {
            const auto node{get_node_ptr()};
            if (node == nullptr) {
                return *this;
            }

            switch (get_state()) {
            case State::Sentinel: {
                *this = node->max();
                return *this;
            }
            case State::MinMax: {
                if (*this == node->min()) {
                    *this = create_sentinel(node);
                } else if (node->cluster_data_ == nullptr) {
                    *this = node->min();
                } else if (const auto hi{*node->cluster_data_->summary_.max()}; node->cluster_data_->unfilled_.contains(hi)) {
                    const auto n8_it{node->cluster_data_->clusters_[node->get_len() - 1].max()};
                    *this = create_resident(node, hi, static_cast<subindex_t>(node->get_len() - 1), n8_it);
                } else {
                    *this = create_implicit(node, index(hi, static_cast<subindex_t>(subnode_t::universe_size() - 1)), static_cast<subindex_t>(node->get_len()));
                }
                return *this;
            }
            case State::Implicit: {
                const auto curr{get_current()};
                const auto [hi, lo] {decompose(curr)};

                if (lo > 0) {
                    *this = create_implicit(node, curr - 1, get_idx());
                } else {
                    move_to_predecessor_cluster(hi);
                }
                return *this;
            }
            case State::Resident: {
                auto n8_it{subiterator_t{data}};
                if (--n8_it != subiterator_t::sentinel()) {
                    *this = create_resident(node, get_hi_byte(), get_idx(), n8_it);
                } else {
                    move_to_predecessor_cluster(get_hi_byte());
                }
                return *this;
            }
            }
            std::unreachable();
        }

        constexpr Iterator operator--(int) {
            Iterator tmp{*this};
            --*this;
            return tmp;
        }

        constexpr bool operator==(Iterator other) const {
            if (is_sentinel() && other.is_sentinel()) {
                return true;
            }
            if (is_sentinel() || other.is_sentinel()) {
                return false;
            }
            return get_current() == other.get_current();
        }

        constexpr bool operator!=(Iterator other) const {
            return !(*this == other);
        }

    private:
        constexpr void move_to_successor_cluster(subindex_t cl) {
            const auto node{get_node_ptr()};
            auto idx{get_idx()};

            if (const auto s_it{node->cluster_data_->summary_.successor(cl)}; s_it != node->cluster_data_->summary_.end()) {
                if (node->cluster_data_->unfilled_.contains(cl)) {
                    ++idx;
                }
                if (const auto s_hi{*s_it}; node->cluster_data_->unfilled_.contains(s_hi)) {
                    const auto n8_it{node->cluster_data_->clusters_[idx].min()};
                    *this = create_resident(node, s_hi, idx, n8_it);
                } else {
                    *this = create_implicit(node, index(s_hi, 0), idx);
                }
            } else {
                *this = node->max();
            }
        }

        constexpr void move_to_predecessor_cluster(subindex_t cl) {
            const auto node{get_node_ptr()};
            auto idx{get_idx()};

            if (const auto p_it{node->cluster_data_->summary_.predecessor(cl)}; p_it != node->cluster_data_->summary_.end()) {
                if (const auto p_hi{*p_it}; idx > 0 && node->cluster_data_->unfilled_.contains(p_hi)) {
                    const auto n8_it{node->cluster_data_->clusters_[--idx].max()};
                    *this = create_resident(node, p_hi, idx, n8_it);
                } else {
                    *this = create_implicit(node, index(p_hi, static_cast<subindex_t>(subnode_t::universe_size() - 1)), idx);
                }
            } else {
                *this = node->min();
            }
        }
    };

private:
    struct cluster_data_t {
        alignas(2 * sizeof(subnode_t))
        subnode_t summary_;
        subnode_t unfilled_;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
        subnode_t clusters_[];
#pragma GCC diagnostic pop

        // resident mask = summary_ & unfilled_
        constexpr inline subnode_t resident_mask() const {
            auto r{summary_};
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
            const auto resident{resident_mask()};
            return static_cast<subindex_t>(resident.count_range({ .hi = static_cast<subindex_t>(x - 1) }));
        }
        constexpr inline subnode_t* find(subindex_t x) {
            return resident_mask().contains(x) ? &clusters_[index_of(x)] : nullptr;
        }
        constexpr inline const subnode_t* find(subindex_t x) const {
            return resident_mask().contains(x) ? &clusters_[index_of(x)] : nullptr;
        }
        constexpr inline std::size_t size() const {
            return summary_.size();
        }
        constexpr inline std::size_t resident_count() const {
            return resident_mask().size();
        }

        // range is inclusive: [lo, hi] to handle 256-element array counts correctly
        // This counts raw bits across the packed resident clusters starting at `lo`/`hi` indices.
        constexpr inline std::size_t count_resident_bits(subindex_t lo, subindex_t hi) const {
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

    static constexpr inline cluster_data_t* create(std::size_t& alloc, std::size_t cap, const cluster_data_t* other, std::size_t other_size) {
        allocator_t a{alloc};
        auto* data = reinterpret_cast<cluster_data_t*>(a.allocate(cap + 2));
        data->summary_ = other->summary_;
        data->unfilled_ = other->unfilled_;
        const auto copy_count{std::min(cap, other_size)};
        std::uninitialized_move_n(
#ifdef __cpp_lib_execution
            std::execution::unseq,
#endif
            other->clusters_, copy_count, data->clusters_
        );
        return data;
    }
    static constexpr inline cluster_data_t* create(std::size_t& alloc, std::size_t cap, subnode_t summary, subnode_t unfilled) {
        const subnode_t dummy[] = {
            summary,
            unfilled,
        };
        return create(alloc, cap, reinterpret_cast<const cluster_data_t*>(&dummy), 0);
    }
    static constexpr inline cluster_data_t* create(std::size_t& alloc, std::size_t cap, subindex_t hi, subnode_t lo) {
        const subnode_t dummy[] = {
            subnode_t::new_with(hi),
            subnode_t::new_all(),
            lo,
        };
        return create(alloc, cap, reinterpret_cast<const cluster_data_t*>(&dummy), 1);
    }

    constexpr inline subnode_t* find(subindex_t x) {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }
    constexpr inline const subnode_t* find(subindex_t x) const {
        return cluster_data_ != nullptr ? cluster_data_->find(x) : nullptr;
    }

    constexpr inline void grow(std::size_t& alloc) {
        const auto len{get_len()};
        const auto cur_cap{get_cap()};
        if (len < cur_cap) {
            return;
        }
        const auto new_cap{std::min(subnode_t::universe_size(), cur_cap + (cur_cap / 4) + 1)};
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
            if (cluster_data_->clusters_[idx].full()) {
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
        return cap_ == 0 ? subnode_t::universe_size() : cap_;
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
            return cluster_data_->resident_count() == 0 ? 0uz : subnode_t::universe_size();
        }
        return len_;
    }
    constexpr inline void set_len(std::size_t s) {
        len_ = static_cast<subindex_t>(s);
    }

    constexpr inline explicit Node16() = default;

public:
    static constexpr inline Node16 new_with(index_t hi, index_t lo) {
        Node16 node{};
        node.key_ = hi;
        node.min_ = lo;
        node.max_ = lo;
        return node;
    }

    static constexpr inline Node16 new_from_node8(subnode_t old_storage, std::size_t& alloc) {
        Node16 node{};
        const auto old_min{*old_storage.min()};
        const auto old_max{*old_storage.max()};

        node.min_ = old_min;
        node.max_ = old_max;

        if (old_storage.remove(old_min) || old_storage.remove(old_max)) {
            return node;
        }

        node.cluster_data_ = create(alloc, 1, 0, old_storage);
        node.set_cap(1);
        node.set_len(1);
        return node;
    }

    // Create a Node16 with all bits set except `x`.
    static constexpr inline Node16 new_all_but(index_t key, index_t x, std::size_t& alloc) {
        static const auto umax{universe_size() - 1};
        Node16 node{};
        node.key_ = key;
        node.min_ = static_cast<index_t>(x == 0 ? 1 : 0);
        node.max_ = static_cast<index_t>(x == umax ? umax - 1 : umax);
        
        const auto summary{subnode_t::new_all()};
        
        const auto [hi, lo] {decompose(x)};
        // unfilled: resident clusters must include cluster 0 and cluster 255 always,
        // plus the cluster containing `x` if it is not one of those.
        auto unfilled{subnode_t::new_with(0)};
        unfilled.insert(static_cast<subindex_t>(subnode_t::universe_size() - 1));
        unfilled.insert(hi);

        const auto len{2uz + (hi != 0 && hi != subnode_t::universe_size() - 1)};
        node.cluster_data_ = create(alloc, len, summary, unfilled);
        node.set_cap(len);

        node.cluster_data_->clusters_[0] = subnode_t::new_all_but(static_cast<subindex_t>(node.min_));
        node.cluster_data_->clusters_[len - 1] = subnode_t::new_all_but(static_cast<subindex_t>(node.max_));
        if (hi == 0) {
            node.cluster_data_->clusters_[0].remove(lo);
        } else if (hi == subnode_t::universe_size() - 1) {
            node.cluster_data_->clusters_[len - 1].remove(lo);
        } else {
            node.cluster_data_->clusters_[1] = subnode_t::new_all_but(lo);
        }
        node.set_len(len);

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
            const auto cap{len == 0 ? 1 : len};
            result.cluster_data_ = create(alloc, cap, cluster_data_, len);
            result.set_cap(cap);
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
        return 1uz + std::numeric_limits<index_t>::max();
    }

    constexpr inline bool full() const {
        // if the min and max are not the universe bounds, we cannot be full
        if (min_ != 0 || max_ != universe_size() - 1 || cluster_data_ == nullptr) {
            return false;
        }
        // we are full if all clusters are implicitly filled except min and max which are stored lazily
        return get_len() == 2uz
            && cluster_data_->summary_.full()
            && cluster_data_->clusters_[0].size() == subnode_t::universe_size() - 1
            && cluster_data_->clusters_[1].size() == subnode_t::universe_size() - 1;
    }

    constexpr inline Iterator min() const {
        return Iterator::create_min_max(this, min_);
    }
    constexpr inline Iterator max() const {
        return Iterator::create_min_max(this, max_);
    }

    constexpr inline Iterator begin() const {
        return min();
    }

    constexpr inline Iterator end() const {
        return Iterator::create_sentinel(this);
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
        if (x < min_ || x > max_) {
            return false;
        }
        if (x == min_) {
            if (cluster_data_ == nullptr) {
                if (max_ == min_) {
                    return true;
                } else {
                    min_ = max_;
                    return false;
                }
            } else {
                const auto min_cluster{*cluster_data_->summary_.min()};
                const auto min_element{cluster_data_->unfilled_.contains(min_cluster) ?
                    *cluster_data_->clusters_[0].min() : static_cast<subindex_t>(0)};
                x = min_ = index(min_cluster, min_element);
            }
        }

        if (x == max_) {
            if (cluster_data_ == nullptr) {
                max_ = min_;
                return false;
            } else {
                const auto max_cluster{*cluster_data_->summary_.max()};
                const auto max_element{cluster_data_->unfilled_.contains(max_cluster) ?
                    *cluster_data_->clusters_[get_len() - 1].max() : static_cast<subindex_t>(subnode_t::universe_size() - 1)};
                x = max_ = index(max_cluster, max_element);
            }
        }

        const auto [h, l] {decompose(x)};

        // If cluster exists implicitly (filled) we need to materialize it as all-but-l and mark it resident
        if (cluster_data_->summary_.contains(h) && !cluster_data_->unfilled_.contains(h)) {
            grow(alloc);
            const auto idx{cluster_data_->index_of(h)};
            const auto size{get_len()};
            if (idx < size) {
                const auto begin{cluster_data_->clusters_ + idx};
                const auto end{cluster_data_->clusters_ + size};
                std::move_backward(begin, end, end + 1);
            }
            cluster_data_->clusters_[idx] = subnode_t::new_all_but(l);
            cluster_data_->unfilled_.insert(h);
            set_len(size + 1);
            return false;
        }

        if (auto* cluster{find(h)}; cluster != nullptr && cluster->remove(l)) {
            if (cluster_data_->summary_.remove(h)) {
                destroy(alloc);
            } else {
                const auto idx{cluster_data_->index_of(h)};
                const auto size{get_len()};
                const auto begin{cluster_data_->clusters_ + idx + 1};
                const auto end{cluster_data_->clusters_ + size};
                std::move(begin, end, begin - 1);
                set_len(size - 1);
            }
        }

        return false;
    }

    constexpr inline bool contains(index_t x) const {
        if (x < min_ || x > max_) {
            return false;
        }
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

    constexpr inline Iterator successor(index_t x) const {
        if (x < min_) {
            return min();
        }
        if (x >= max_) {
            return end();
        }

        if (cluster_data_ == nullptr) {
            return max();
        }

        const auto [h, l] {decompose(x)};

        if (cluster_data_->summary_.contains(h)) {
            if (!cluster_data_->unfilled_.contains(h)) {
                if (l < static_cast<subindex_t>(subnode_t::universe_size() - 1)) {
                    return Iterator::create_implicit(this, index(h, static_cast<subindex_t>(l + 1)), cluster_data_->index_of(h));
                }
            } else if (const auto* cluster{find(h)}; cluster != nullptr && l < *cluster->max()) {
                return Iterator::create_resident(this, h, static_cast<subindex_t>(cluster - &cluster_data_->clusters_[0]), cluster->successor(l));
            }
        }

        if (const auto s_it{cluster_data_->summary_.successor(h)}; s_it != cluster_data_->summary_.end()) {
            const auto succ{*s_it};
            const auto idx{cluster_data_->index_of(succ)};
            if (!cluster_data_->unfilled_.contains(succ)) {
                return Iterator::create_implicit(this, index(succ, 0), idx);
            }
            return Iterator::create_resident(this, succ, idx, cluster_data_->clusters_[idx].min());
        }

        return max();
    }

    constexpr inline Iterator predecessor(index_t x) const {
        if (x > max_) {
            return max();
        }
        if (x <= min_) {
            return end();
        }

        if (cluster_data_ == nullptr) {
            return min();
        }

        const auto [h, l] {decompose(x)};

        if (cluster_data_->summary_.contains(h)) {
            if (!cluster_data_->unfilled_.contains(h)) {
                if (l > static_cast<subindex_t>(0)) {
                    return Iterator::create_implicit(this, index(h, static_cast<subindex_t>(l - 1)), cluster_data_->index_of(h));
                }
            } else if (const auto* cluster{find(h)}; cluster != nullptr && l > *cluster->min()) {
                return Iterator::create_resident(this, h, static_cast<subindex_t>(cluster - &cluster_data_->clusters_[0]), cluster->predecessor(l));
            }
        }

        if (const auto p_it{cluster_data_->summary_.predecessor(h)}; p_it != cluster_data_->summary_.end()) {
            const auto pred{*p_it};
            const auto idx{cluster_data_->index_of(pred)};
            if (!cluster_data_->unfilled_.contains(pred)) {
                return Iterator::create_implicit(this, index(pred, static_cast<subindex_t>(subnode_t::universe_size() - 1)), idx);
            }
            return Iterator::create_resident(this, pred, idx, cluster_data_->clusters_[idx].max());
        }

        return min();
    }

    constexpr inline std::size_t size() const {
        return count_range({});
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

        const auto [lcl, lidx] {decompose(lo)};
        const auto [hcl, hidx] {decompose(hi)};
        if (lcl == hcl) {
            if (cluster_data_->summary_.contains(lcl)) {
                if (!cluster_data_->unfilled_.contains(lcl)) {
                    acc += 1uz + hidx - lidx;
                } else {
                    acc += find(lcl)->count_range({ .lo = lidx, .hi = hidx });
                }
            }
            return acc;
        }

        // left cluster partial
        if (cluster_data_->summary_.contains(lcl)) {
            if (!cluster_data_->unfilled_.contains(lcl)) {
                acc += subnode_t::universe_size() - lidx;
            } else {
                acc += find(lcl)->count_range({ .lo = lidx });
            }
        }

        // right cluster partial
        if (cluster_data_->summary_.contains(hcl)) {
            if (!cluster_data_->unfilled_.contains(hcl)) {
                acc += 1uz + hidx;
            } else {
                acc += find(hcl)->count_range({ .hi = hidx });
            }
        }

        const auto resident_mask{cluster_data_->resident_mask()};
        const auto from{resident_mask.successor(lcl)};
        const auto to{resident_mask.predecessor(hcl)};
        if (from != resident_mask.end() && to != resident_mask.end() && *from <= *to) {
            acc += cluster_data_->count_resident_bits(
                cluster_data_->index_of(*from),
                cluster_data_->index_of(*to)
            );
        }

        if (lcl + 1 <= hcl - 1) {
            // need to check return value of not_inplace here because there might not be any nonresident clusters
            if (auto nonresident_mask{cluster_data_->unfilled_}; !nonresident_mask.not_inplace()) {
                acc += subnode_t::universe_size() * nonresident_mask.count_range({
                    .lo = static_cast<subindex_t>(lcl + 1),
                    .hi = static_cast<subindex_t>(hcl - 1)
                });
            }
        }
        return acc;
    }

    constexpr inline std::uint16_t key() const {
        return key_;
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

    inline void serialize(std::string &out) const {
        write_u16(out, min_);
        write_u16(out, max_);

        if (cluster_data_ == nullptr) {
            write_u16(out, 0);
            return;
        }

        // encode resident count + 1 so 0 remains sentinel for no cluster data
        const auto resident_count{cluster_data_->resident_count()};
        write_u16(out, static_cast<std::uint16_t>(resident_count + 1));

        cluster_data_->summary_.serialize(out);
        cluster_data_->unfilled_.serialize(out);

        for (auto idx{0uz}; idx < resident_count; ++idx) {
            cluster_data_->clusters_[idx].serialize(out);
        }
    }

    static inline Node16 deserialize(std::string_view buf, std::size_t &pos, index_t key, std::size_t &alloc) {
        Node16 node{};
        node.key_ = key;
        node.min_ = read_u16(buf, pos);
        node.max_ = read_u16(buf, pos);

        const auto raw_len{read_u16(buf, pos)};
        if (raw_len == 0) {
            return node;
        }

        // capacity may never be 0 because that is interpreted as 256. if raw_len == 1, allocate room for an unused dummy cluster
        const auto dummy_resident{raw_len == 1};
        const auto resident_count{static_cast<std::size_t>(raw_len - !dummy_resident)};

        const auto summary{subnode_t::deserialize(buf, pos)};
        const auto unfilled{subnode_t::deserialize(buf, pos)};
        node.cluster_data_ = create(alloc, resident_count, summary, unfilled);

        if (!dummy_resident) {
            for (auto idx{0uz}; idx < resident_count; ++idx) {
                node.cluster_data_->clusters_[idx] = subnode_t::deserialize(buf, pos);
            }
        }

        node.set_cap(resident_count);
        node.set_len(resident_count - dummy_resident);
        return node;
    }

    constexpr inline bool not_inplace(std::size_t& alloc) {
        const auto s_min{min_};
        const auto s_max{max_};

        if (cluster_data_ == nullptr) {
            *this = new_all_but(key_, s_min, alloc);
            remove(s_max, alloc);
            return false;
        }

        const auto [nh, nl] {decompose(s_min)};
        const auto [xh, xl] {decompose(s_max)};
        emplace(nh, nl, alloc);
        emplace(xh, xl, alloc);

        auto& s_summary{cluster_data_->summary_};
        auto& s_unfilled{cluster_data_->unfilled_};
        auto* clusters{cluster_data_->clusters_};

        auto compl_summary{s_summary};
        compl_summary.not_inplace();
        compl_summary.or_inplace(cluster_data_->resident_mask());
        s_unfilled = s_summary; // ensure all clusters that were previously present are set in the compl unfilled
        s_summary = compl_summary; // ensure all resident clusters are set in the compl summary

        for (auto h{0uz}; h < get_len(); ++h) {
            clusters[h].not_inplace();
        }

        // if 0 wasn't the old min, then it is now the new min,
        // similarly if 255 wasn't the old max, then it is now the new max,
        // we can safely set min and max to universe bounds and remove old min and max from clusters
        // without risk of removing the new min and max
        min_ = 0;
        max_ = universe_size() - 1;
        remove(min_, alloc);
        remove(max_, alloc);
        return false;
    }

    constexpr inline bool or_inplace(const Node16& other, std::size_t& alloc) {
        insert(other.min_, alloc);
        insert(other.max_, alloc);

        if (other.cluster_data_ == nullptr) {
            return false;
        }

        if (cluster_data_ == nullptr) {
            const auto len{other.get_len()};
            const auto cap{len == 0 ? 1 : len};
            cluster_data_ = create(alloc, cap, other.cluster_data_, len);
            set_cap(cap);
            set_len(len);

            return false;
        }

        const auto& s_summary{cluster_data_->summary_};
        const auto& s_unfilled{cluster_data_->unfilled_};
        const auto s_resident{cluster_data_->resident_mask()};
        const auto* s_clusters{cluster_data_->clusters_};
        const auto& o_summary{other.cluster_data_->summary_};
        const auto& o_unfilled{other.cluster_data_->unfilled_};
        const auto o_resident{other.cluster_data_->resident_mask()};
        const auto* o_clusters{other.cluster_data_->clusters_};

        // get a conservative set of clusters that can remain resident, and predict a resident upper bound
        auto merge_summary{s_summary};
        merge_summary.or_inplace(o_summary);
        auto merge_unfilled{s_unfilled};
        merge_unfilled.and_inplace(o_unfilled); // no need to check return value, cannot be empty since min and max are not stored in clusters
        auto merge_resident{merge_summary};
        if (merge_resident.and_inplace(merge_unfilled)) {
            // all clusters are implicitly filled, no resident clusters
            cluster_data_->summary_ = merge_summary;
            cluster_data_->unfilled_ = merge_unfilled;
            set_len(0);
            return false;
        }
        const auto new_size{merge_resident.size()};

        // If predicted upper limit of resident clusters fits in current capacity, use original clusters and do in-place merge
        auto* merge_data{(new_size <= get_cap()) ? cluster_data_ : create(alloc, new_size, merge_summary, merge_unfilled)};
        auto* merge_clusters{merge_data->clusters_};

        auto i{0uz};
        auto j{0uz};
        auto k{0uz};
        for (const auto h : merge_summary) {
            const auto re_s{s_resident.contains(h)};
            const auto re_o{o_resident.contains(h)};

            if (re_s && re_o) {
                auto tmp{s_clusters[i++]};
                tmp.or_inplace(o_clusters[j++]);
                if (tmp.full()) {
                    merge_unfilled.remove(h);
                } else {
                    merge_clusters[k++] = tmp;
                }
            } else if (re_s) {
                if (!o_summary.contains(h)) {
                    merge_clusters[k++] = s_clusters[i];
                }
                i++;
            } else if (re_o) {
                if (!s_summary.contains(h)) {
                    merge_clusters[k++] = o_clusters[j];
                }
                j++;
            }
        }
        if (merge_data != cluster_data_) {
            destroy(alloc);
            cluster_data_ = merge_data;
            set_cap(new_size);
        }
        cluster_data_->summary_ = merge_summary;
        cluster_data_->unfilled_ = merge_unfilled;
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

        const auto& s_summary{cluster_data_->summary_};
        const auto& s_unfilled{cluster_data_->unfilled_};
        const auto s_resident{cluster_data_->resident_mask()};
        const auto* s_clusters{cluster_data_->clusters_};
        const auto& o_summary{other.cluster_data_->summary_};
        const auto& o_unfilled{other.cluster_data_->unfilled_};
        const auto o_resident{other.cluster_data_->resident_mask()};
        const auto* o_clusters{other.cluster_data_->clusters_};

        // Predict resident clusters: clusters in the intersection that are resident in at least one side
        auto int_unfilled{s_unfilled};
        int_unfilled.or_inplace(o_unfilled);
        auto resident{int_summary};
        if (resident.and_inplace(int_unfilled)) {
            // intersection has no resident clusters, only implicit full clusters
            // check if min/max require materialization and exit
            std::optional<subnode_t> min_c_o{std::nullopt};
            std::optional<subnode_t> max_c_o{std::nullopt};
            const auto min_hi{*int_summary.min()};
            const auto max_hi{*int_summary.max()};
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
                auto* int_data{(new_len <= get_cap()) ? cluster_data_ : create(alloc, new_len, int_summary, int_unfilled)};
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

        // If predicted resident clusters exceed capacity, allocate a new cluster_data_t and write into it
        auto* int_data{(resident_count <= get_cap()) ? cluster_data_ : create(alloc, resident_count, int_summary, int_unfilled)};
        auto* int_clusters{int_data->clusters_};

        auto i{0uz};
        auto j{0uz};
        auto k{0uz};
        auto merge_summary{s_summary};
        merge_summary.or_inplace(o_summary);

        for (const auto h : merge_summary) {
            const bool in_s{s_resident.contains(h)};
            const bool in_o{o_resident.contains(h)};

            // cluster not in intersection -> skip
            if (!int_summary.contains(h)) {
                if (in_s) {
                    i++;
                }
                if (in_o) {
                    j++;
                }
                continue;
            }

            // both implicit-filled -> result is implicitly-filled (full).
            if (!resident.contains(h)) {
                if (in_s) {
                    i++;
                }
                if (in_o) {
                    j++;
                }
                if (min_out) {
                    min_out = false;
                    min_ = index(h, static_cast<subindex_t>(0));
                    new_min = std::make_optional(min_);
                    int_clusters[k++] = subnode_t::new_all_but(0);
                    int_unfilled.insert(h);
                }
                continue;
            }

            if (auto tmp{subnode_t::new_all()};
                (in_s && tmp.and_inplace(s_clusters[i++])) ||
                (in_o && tmp.and_inplace(o_clusters[j++]))) {
                if (int_summary.remove(h)) {
                    // last element removed -> update min/max and return
                    if (int_data != cluster_data_) {
                        allocator_t{alloc}.deallocate(reinterpret_cast<subnode_t*>(int_data), resident_count + 2);
                    }
                    return update_minmax();
                }
                continue;
            } else {
                int_clusters[k++] = tmp;
            }

            // cluster is non-empty. check if we need to update min_. only happens once, at the 0'th cluster.
            // which might not end up being the true 0'th cluster if it gets removed here.
            if (min_out) {
                min_out = false;
                min_ = index(h, *int_clusters[0].min());
                new_min = std::make_optional(min_);
                if (int_clusters[0].remove(static_cast<subindex_t>(min_))) {
                    // cluster became empty after removing min
                    --k;
                    if (int_summary.remove(h)) {
                        // node is now clusterless, but not empty since min_ at least exists.
                        // update max_ and exit.
                        if (int_data != cluster_data_) {
                            allocator_t{alloc}.deallocate(reinterpret_cast<subnode_t*>(int_data), resident_count + 2);
                        }
                        destroy(alloc);
                        max_ = new_max.has_value() ? new_max.value() : min_;
                        return false;
                    }
                }
            }
        }

        int_data->summary_ = int_summary;
        int_data->unfilled_ = int_unfilled;
        if (int_data != cluster_data_) {
            destroy(alloc);
            cluster_data_ = int_data;
            set_cap(resident_count);
        }

        if (!new_max.has_value()) {
            if (const auto max_hi{*cluster_data_->summary_.max()}; !resident.contains(max_hi)) {
                max_ = index(max_hi, static_cast<subindex_t>(255));
                cluster_data_->unfilled_.insert(max_hi);
                cluster_data_->clusters_[k++] = subnode_t::new_all_but(255);
            } else if (max_ = index(max_hi, *cluster_data_->clusters_[k - 1].max());
                       max_ != s_max && cluster_data_->clusters_[k - 1].remove(static_cast<subindex_t>(max_))) {
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
        auto update_minmax = [&] {
            // self must contain o_min if s_min > o_min due to the insert at the top of the function
            // we handle the case where s_min == o_min below. this handles the case where s_min < o_min
            // we do not need to check the return value here, as removing o_min cannot empty this node as s_min must exist
            if (s_min < o_min && max_ != o_min) {
                if (contains(o_min)) {
                    remove(o_min, alloc);
                } else {
                    insert(o_min, alloc);
                }
            }
            if (s_max > o_max && min_ != o_max) {
                if (contains(o_max)) {
                    remove(o_max, alloc);
                } else {
                    insert(o_max, alloc);
                }
            }

            // if o contains s_min then either it was equal to o_min or in o_clusters.
            // if it was equal to o_min, we pull up the next minimum from s_clusters, which by now we know is not in o
            // if it was in one of the clusters, that means o_min < s_min, and s_min was also pushed into s_clusters.
            // that value will not exist in s, so we do not need to remove it
            // if these removals empty the node, that means the node contained only s_min and s_max, and cluster_data was already null
            return (s_min == o_min && remove(s_min, alloc)) ||
                   (s_max == o_max && remove(s_max, alloc));
        };

        if (other.cluster_data_ == nullptr) {
            // Only need to adjust min and max
        } else if (cluster_data_ == nullptr) {
            const auto len{other.get_len()};
            const auto cap{len == 0 ? 1 : len};
            cluster_data_ = create(alloc, cap, other.cluster_data_, len);
            set_cap(cap);
            set_len(len);
        } else {
            const auto& s_summary{cluster_data_->summary_};
            const auto& s_unfilled{cluster_data_->unfilled_};
            const auto s_resident{cluster_data_->resident_mask()};
            const auto* s_clusters{cluster_data_->clusters_};
            const auto& o_summary{other.cluster_data_->summary_};
            const auto& o_unfilled{other.cluster_data_->unfilled_};
            const auto o_resident{other.cluster_data_->resident_mask()};
            const auto* o_clusters{other.cluster_data_->clusters_};

            // pre compute maximal merged residency. if the size fits in capacity, we can do everything in place
            // this probably isn't the common case, but it's worth optimizing for nonetheless to avoid unnecessary allocations and copies
            auto diff_summary{s_summary};
            diff_summary.or_inplace(o_summary);
            auto diff_unfilled{s_unfilled};
            diff_unfilled.or_inplace(o_unfilled);
            auto diff_resident{s_resident};
            diff_resident.or_inplace(o_resident);
            const auto resident_count{diff_resident.size()};

            auto* diff_data{(resident_count <= get_cap()) ? cluster_data_ : create(alloc, resident_count, diff_summary, diff_unfilled)};
            auto* diff_clusters{diff_data->clusters_};

            auto i{0uz};
            auto j{0uz};
            auto k{0uz};
            for (const auto h : diff_summary) {
                const bool in_s{s_summary.contains(h)};
                const bool in_o{o_summary.contains(h)};
                const bool re_s{s_resident.contains(h)};
                const bool re_o{o_resident.contains(h)};

                if (re_s && re_o) {
                    diff_clusters[k] = s_clusters[i++];
                    if (diff_clusters[k].xor_inplace(o_clusters[j++])) {
                        if (diff_summary.remove(h)) {
                            if (diff_data != cluster_data_) {
                                allocator_t{alloc}.deallocate(reinterpret_cast<subnode_t*>(diff_data), resident_count + 2);
                            }
                            destroy(alloc);
                            return update_minmax();
                        }
                    } else if (diff_clusters[k].full()) {
                        diff_unfilled.remove(h);
                    } else {
                        ++k;
                    }
                } else if (re_s) {         // resident in s only
                    diff_clusters[k] = s_clusters[i++];
                    if (in_o) {            // implicit in o
                        diff_clusters[k].not_inplace();
                    }
                    ++k;
                } else if (re_o) {         // resident in o only
                    diff_clusters[k] = o_clusters[j++];
                    if (in_s) {            // implicit in s
                        diff_clusters[k].not_inplace();
                    }
                    ++k;
                } else if (in_s && in_o) { // implicit in s and o. result is empty
                    if (diff_summary.remove(h)) {
                        if (diff_data != cluster_data_) {
                            allocator_t{alloc}.deallocate(reinterpret_cast<subnode_t*>(diff_data), resident_count + 2);
                        }
                        destroy(alloc);
                        return update_minmax();
                    }
                    diff_unfilled.insert(h);
                } else {                   // implicit in s xor o. remains implicit
                    diff_unfilled.remove(h);
                }
            }

            diff_data->summary_ = diff_summary;
            diff_data->unfilled_ = diff_unfilled;
            if (diff_data != cluster_data_) {
                destroy(alloc);
                cluster_data_ = diff_data;
                set_cap(resident_count);
            }
            set_len(k);
        }
        return update_minmax();
    }

    struct Eq {
        using is_transparent = void;
        constexpr inline bool operator()(const Node16& lhs, const Node16& rhs) const {
            return lhs.key_ == rhs.key_;
        }
        constexpr inline bool operator()(const index_t lhs, const Node16& rhs) const {
            return lhs == rhs.key_;
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
