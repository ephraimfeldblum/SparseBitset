/**
 * @brief van Emde Boas Tree implementation for dynamic bitsets
 * reference: https://en.wikipedia.org/wiki/Van_Emde_Boas_tree
 *
 * This implementation uses a recursive node cluster structure to store the elements.
 * Each node is associated with a summary structure to indicate which clusters contain elements.
 *
 * The tree is extremely flat and can be visualized as shown below:
 * Node<log U = 32>
 * ┌───────────────────────────────┐
 * | min, max: u32                 | ← Lazily propagated. Not inserted into clusters.
 * | cluster_data: * {             | ← Lazily constructed only if non-empty.
 * |   summary : Node<16>          | ← Tracks which clusters are non-empty.
 * |   clusters: HashSet<Node<16>> | ← Up to √U clusters, each of size √U. Use HashSet to exploit padding bits in Node16 structure.
 * | }           |                 |
 * └─────────────|─────────────────┘
 * Node<16>      ▼
 * ┌───────────────────────────────┐
 * | key: u16                      | ← Store which cluster this node belongs to directly in padding bytes.
 * | min, max: u16                 |
 * | cap, len: u8                  | ← Capacity and length of clusters array. 0 represents 256.
 * | cluster_data: * {             |
 * |   summary : Node<8>           | ← Used to index into clusters in constant time. Requires sorted clusters.
 * |   unfilled: Node<8>           | ← Tracks which clusters are not full. Full clusters are implicit and not stored.
 * |   clusters: FAM<Node<8>>      | ← Up to 256 elements. Flexible array is more cache-friendly than HashMap.
 * | }           |                 |
 * └─────────────|─────────────────┘
 * Node<8>       ▼
 * ┌───────────────────────────────┐
 * | bits: Array<u64, 4>           | ← 256 bits, SIMD-friendly.
 * └───────────────────────────────┘
 */

#ifndef VEBTREE_HPP
#define VEBTREE_HPP

#include <cstddef>       // std::ptrdiff_t, std::size_t
#include <iterator>      // std::bidirectional_iterator_tag
#include <optional>      // std::nullopt, std::optional
#include <utility>       // std::exchange, std::move, std::unreachable
#include <variant>       // std::holds_alternative, std::monostate, std::variant, std::visit
#include <vector>        // std::vector

#include "allocator/tracking_allocator.hpp"
#include "VebCommon.hpp"
#include "node8.hpp"
#include "node16.hpp"
#include "node32.hpp"
#include "node64.hpp"

static_assert(sizeof(Node8) == 32, "Node8 size is incorrect");
static_assert(sizeof(Node16) == 16, "Node16 size is incorrect");
static_assert(sizeof(Node32) == 16, "Node32 size is incorrect");
static_assert(sizeof(Node64) == 24, "Node64 size is incorrect");

/**
 * @brief van Emde Boas Tree with size-specific node implementations
 *
 * A van Emde Boas tree that automatically selects the appropriate node type
 * based on the universe size:
 * - Node8 for universe < 256 (2^8)
 * - Node16 for universe < 65,536 (2^16)
 * - Node32 for universe < 4,294,967,296 (2^32)
 * - Node64 for universe < 9,223,372,036,854,775,808 (2^63)
 */
struct VebTree {
private:
    using StorageType = std::variant<std::monostate, Node8, Node16, Node32, Node64>;
    StorageType storage_{std::monostate{}};
    std::size_t allocated_{sizeof *this};
    std::size_t max_seen_{0};

    static inline StorageType create_storage(std::size_t x) {
        if (x < Node8::universe_size()) {
            return Node8::new_with(static_cast<Node8::index_t>(x));
        } else if (x < Node16::universe_size()) {
            return Node16::new_with(0, static_cast<Node16::index_t>(x));
        } else if (x < Node32::universe_size()) {
            return Node32::new_with(static_cast<Node32::index_t>(x));
        } else {
            return Node64::new_with(static_cast<Node64::index_t>(x));
        }
    }

    inline void grow_storage(std::size_t x, std::size_t& alloc) {
        std::visit(
            overload{
                [&](Node8&& old_storage) {
                    storage_ = Node16::new_from_node8(old_storage, alloc);
                },
                [&](Node16&& old_storage) {
                    storage_ = Node32::new_from_node16(std::move(old_storage), alloc);
                },
                [&](Node32&& old_storage) {
                    storage_ = Node64::new_from_node32(std::move(old_storage), alloc);
                },
                [](auto&&) { std::unreachable(); },
            },
            std::move(storage_));

        insert(x);
    }

    inline void destroy_storage() {
        std::visit(
            overload{
                [](std::monostate) {},
                [](Node8) {},
                [&](auto& s) {
                    s.destroy(allocated_);
                },
            },
            storage_
        );
    }

public:
    ~VebTree() {
        destroy_storage();
    }

    /**
     * @brief Bidirectional iterator for VebTree
     * 
     * A bidirectional iterator that allows forward and backward traversal of elements
     * stored in a VEB tree. The iterator maintains cache pointers to the current node
     * level (Node8, Node16, Node32, or Node64) to optimize successor/predecessor lookups.
     * 
     * @details
     * The iterator supports the following operations:
     * - Increment (++): Move to the next element in ascending order
     * - Decrement (--): Move to the previous element in descending order
     * - Dereference (*): Access the current element value
     * - Comparison (==, !=): Compare two iterators for equality
     * 
     * Special sentinels are used to represent begin and end positions:
     * - End sentinel: current_ == SIZE_MAX, all node pointers are nullptr
     * - Reverse-end sentinel: current_ == 0, all node pointers are nullptr
     * 
     * @note The iterator caches pointers to the currently active nodes to avoid
     * repeated tree traversals during iteration. These pointers are updated as
     * the iterator moves through the tree.
     * 
     * @warning The iterator becomes invalid if the underlying pointers are modified
     * during iteration. Any modification to the tree may invalidate iterators.
     */
    struct Iterator {
        const VebTree* tree_;
        std::size_t current_;
        
        const Node64* node64_{nullptr};
        const Node32* node32_{nullptr};
        const Node16* node16_{nullptr};
        const Node8*  node8_ {nullptr};

    public:
        using iterator_concept = std::bidirectional_iterator_tag;
        using iterator_category = std::bidirectional_iterator_tag;
        using value_type = std::size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const std::size_t*;
        using reference = const std::size_t&;

        inline constexpr bool is_sentinel() const {
            return node8_ == nullptr && node16_ == nullptr && 
                   node32_ == nullptr && node64_ == nullptr;
        }

        inline constexpr bool is_end_sentinel() const {
            return is_sentinel() && current_ == SIZE_MAX;
        }

        inline constexpr bool is_rend_sentinel() const {
            return is_sentinel() && current_ == 0;
        }

        inline void init_nodes_from_storage() {
            if (tree_ != nullptr) {
                std::visit(overload{
                    [&](const Node8& s) { node8_ = &s; },
                    [&](const Node16& s) { node16_ = &s; },
                    [&](const Node32& s) { node32_ = &s; },
                    [&](const Node64& s) { node64_ = &s; },
                    [](std::monostate) {}
                }, tree_->storage_);
            }
        }

        inline explicit Iterator(const VebTree* tree, std::size_t current, bool is_sentinel = false)
            : tree_{tree}, current_{current} {
            if (!is_sentinel) {
                init_nodes_from_storage();
            }
        }

    public:
        inline Iterator& operator++() {
            if (is_end_sentinel() || tree_ == nullptr) {
                return *this;
            }

            if (is_rend_sentinel()) {
                auto min_val = tree_->min();
                if (min_val.has_value()) {
                    current_ = min_val.value();
                    init_nodes_from_storage();
                }
                return *this;
            }

            if (node8_ != nullptr) {
                const auto n8idx{static_cast<Node8::index_t>(current_)};
                if (const auto succ{node8_->successor(n8idx)}; succ != node8_->end()) {
                    current_ &= ~0xFF;
                    current_ |= *succ;
                    return *this;
                }
            }

            if (node16_ != nullptr) {
                const auto n16idx{static_cast<Node16::index_t>(current_)};
                if (const auto succ{node16_->successor(n16idx)}; succ != node16_->end()) {
                    current_ &= ~0xFFFF;
                    current_ |= *succ;
                    const auto [high, low] {Node16::decompose(static_cast<Node16::index_t>(current_))};
                    if (succ != node16_->min() && succ != node16_->max() && node16_->cluster_data_ != nullptr) {
                        node8_ = node16_->cluster_data_->find(high);
                    } else {
                        node8_ = nullptr;
                    }
                    return *this;
                }
            }

            if (node32_ != nullptr) {
                const auto n32idx{static_cast<Node32::index_t>(current_)};
                if (const auto succ{node32_->successor(n32idx)}; succ.has_value()) {
                    current_ &= ~0xFFFFFFFF;
                    current_ |= succ.value();
                    const auto [high, low] {Node32::decompose(static_cast<Node32::index_t>(current_))};
                    node8_ = nullptr;
                    if (succ.value() != node32_->min() && succ.value() != node32_->max() && node32_->cluster_data_ != nullptr) {
                        if (const auto it{node32_->cluster_data_->clusters.find(high)}; it != node32_->cluster_data_->clusters.end()) {
                            node16_ = &*it;
                        } else {
                            node16_ = nullptr;
                        }
                    } else {
                        node16_ = nullptr;
                    }
                    return *this;
                }
            }

            if (node64_ != nullptr) {
                if (const auto succ{node64_->successor(current_)}; succ.has_value()) {
                    current_ = succ.value();
                    const auto [high, low] {Node64::decompose(current_)};
                    node8_ = nullptr;
                    node16_ = nullptr;
                    if (succ.value() != node64_->min() && succ.value() != node64_->max() && node64_->cluster_data_ != nullptr) {
                        if (const auto it{node64_->cluster_data_->clusters.find(high)}; it != node64_->cluster_data_->clusters.end()) {
                            node32_ = &it->second;
                        } else {
                            node32_ = nullptr;
                        }
                    } else {
                        node32_ = nullptr;
                    }
                    return *this;
                }
            }

            current_ = SIZE_MAX;
            node8_ = nullptr;
            node16_ = nullptr;
            node32_ = nullptr;
            node64_ = nullptr;
            return *this;
        }

        inline Iterator operator++(int) {
            Iterator tmp{*this};
            ++*this;
            return tmp;
        }

        inline Iterator& operator--() {
            if (is_rend_sentinel() || tree_ == nullptr) {
                return *this;
            }

            if (is_end_sentinel()) {
                auto max_val = tree_->max();
                if (max_val.has_value()) {
                    current_ = max_val.value();
                    init_nodes_from_storage();
                }
                return *this;
            }

            if (node8_ != nullptr) {
                const auto n8idx{static_cast<Node8::index_t>(current_)};
                if (const auto succ{node8_->predecessor(n8idx)}; succ != node8_->end()) {
                    current_ &= ~0xFF;
                    current_ |= *succ;
                    return *this;
                }
            }

            if (node16_ != nullptr) {
                const auto n16idx{static_cast<Node16::index_t>(current_)};
                if (const auto succ{node16_->predecessor(n16idx)}; succ != node16_->end()) {
                    current_ &= ~0xFFFF;
                    current_ |= *succ;
                    const auto [high, low] {Node16::decompose(static_cast<Node16::index_t>(current_))};
                    if (succ != node16_->min() && succ != node16_->max() && node16_->cluster_data_ != nullptr) {
                        node8_ = node16_->cluster_data_->find(high);
                    } else {
                        node8_ = nullptr;
                    }
                    return *this;
                }
            }

            if (node32_ != nullptr) {
                const auto n32idx{static_cast<Node32::index_t>(current_)};
                if (const auto succ{node32_->predecessor(n32idx)}; succ.has_value()) {
                    current_ &= ~0xFFFFFFFF;
                    current_ |= succ.value();
                    const auto [high, low] {Node32::decompose(static_cast<Node32::index_t>(current_))};
                    node8_ = nullptr;
                    if (succ.value() != node32_->min() && succ.value() != node32_->max() && node32_->cluster_data_ != nullptr) {
                        if (const auto it{node32_->cluster_data_->clusters.find(high)}; it != node32_->cluster_data_->clusters.end()) {
                            node16_ = &*it;
                        } else {
                            node16_ = nullptr;
                        }
                    } else {
                        node16_ = nullptr;
                    }
                    return *this;
                }
            }

            if (node64_ != nullptr) {
                if (const auto succ{node64_->predecessor(current_)}; succ.has_value()) {
                    current_ = succ.value();
                    const auto [high, low] {Node64::decompose(current_)};
                    node8_ = nullptr;
                    node16_ = nullptr;
                    if (succ.value() != node64_->min() && succ.value() != node64_->max() && node64_->cluster_data_ != nullptr) {
                        if (const auto it{node64_->cluster_data_->clusters.find(high)}; it != node64_->cluster_data_->clusters.end()) {
                            node32_ = &it->second;
                        } else {
                            node32_ = nullptr;
                        }
                    } else {
                        node32_ = nullptr;
                    }
                    return *this;
                }
            }

            current_ = 0uz;
            node8_ = nullptr;
            node16_ = nullptr;
            node32_ = nullptr;
            node64_ = nullptr;
            return *this;
        }

        inline Iterator operator--(int) {
            Iterator tmp{*this};
            --*this;
            return tmp;
        }

        constexpr bool operator==(Iterator other) const {
            if (is_sentinel() && other.is_sentinel()) {
                return current_ == other.current_;
            }
            if (is_sentinel() || other.is_sentinel()) {
                return false;
            }
            return current_ == other.current_;
        }

        constexpr bool operator!=(Iterator other) const {
            return !(*this == other);
        }

        constexpr reference operator*() const {
            return current_;
        }
    };

    /**
     * @brief Constructs an empty VEB tree
     */
    inline explicit VebTree() {}

    inline explicit VebTree(const VebTree& other)
        : max_seen_{other.max_seen_} {
        storage_ = std::visit(overload{
            [](std::monostate) { return StorageType{}; },
            [](const Node8& s) { return StorageType{s}; },
            [&](const auto& s) { return StorageType{s.clone(allocated_)}; },
        }, other.storage_);
    }

    inline VebTree(VebTree&& other) noexcept
        : storage_{std::exchange(other.storage_, std::monostate{})}
        , allocated_{std::exchange(other.allocated_, sizeof *this)}
        , max_seen_{std::exchange(other.max_seen_, 0)} {
    }

    inline VebTree& operator=(VebTree&& other) noexcept {
        if (this != &other) {
            destroy_storage();
            storage_ = std::exchange(other.storage_, std::monostate{});
            allocated_ = std::exchange(other.allocated_, sizeof *this);
            max_seen_ = std::exchange(other.max_seen_, 0);
        }
        return *this;
    }

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
                    storage_ = create_storage(x);
                },
                [&](Node8& s) {
                    if (x >= s.universe_size()) {
                        grow_storage(x, allocated_);
                    } else {
                        s.insert(static_cast<Node8::index_t>(x));
                    }
                },
                [&](auto& s) {
                    if (x >= s.universe_size()) {
                        grow_storage(x, allocated_);
                    } else {
                        s.insert(static_cast<index_t<decltype(s)>>(x), allocated_);
                    }
                },
            }, storage_);
        if (x > max_seen_) {
            max_seen_ = x;
        }
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
                [&](Node8& s) {
                    if (x < s.universe_size() && s.remove(static_cast<Node8::index_t>(x))) {
                        storage_ = std::monostate{};
                    }
                },
                [&](auto& s) {
                    if (x < s.universe_size() && s.remove(static_cast<index_t<decltype(s)>>(x), allocated_)) {
                        s.destroy(allocated_);
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
                    return x < s.universe_size() && s.contains(static_cast<index_t<decltype(s)>>(x));
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
        return std::visit(
            overload{
                [](std::monostate) -> std::optional<std::size_t> { return std::nullopt; },
                [&](const Node8& s) -> std::optional<std::size_t> {
                    if (x >= s.universe_size()) {
                        return std::nullopt;
                    }
                    if (x < *s.min()) {
                        return std::make_optional<std::size_t>(*s.min());
                    }
                    const auto succ = s.successor(static_cast<Node8::index_t>(x));
                    return succ != s.end() ? std::make_optional<std::size_t>(*succ) : std::nullopt;
                },
                [&](const Node16& s) -> std::optional<std::size_t> {
                    if (x >= s.universe_size()) {
                        return std::nullopt;
                    }
                    if (x < *s.min()) {
                        return std::make_optional<std::size_t>(*s.min());
                    }
                    const auto succ = s.successor(static_cast<Node16::index_t>(x));
                    return succ != s.end() ? std::make_optional<std::size_t>(*succ) : std::nullopt;
                },
                [&](const auto& s) -> std::optional<std::size_t> {
                    if (x >= s.universe_size()) {
                        return std::nullopt;
                    }
                    if (x < s.min()) {
                        return std::make_optional<std::size_t>(s.min());
                    }
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
        return std::visit(
            overload{
                [](std::monostate) -> std::optional<std::size_t> { return std::nullopt; },
                [&](const Node8& s) -> std::optional<std::size_t> {
                    if (x == 0) {
                        return std::nullopt;
                    }
                    if (const auto max{*s.max()}; x > max) {
                        return std::make_optional<std::size_t>(max);
                    }
                    const auto pred = s.predecessor(static_cast<Node8::index_t>(x));
                    return pred != s.end() ? std::make_optional<std::size_t>(*pred) : std::nullopt;
                },
                [&](const Node16& s) -> std::optional<std::size_t> {
                    if (x == 0) {
                        return std::nullopt;
                    }
                    if (const auto max{*s.max()}; x > max) {
                        return std::make_optional<std::size_t>(max);
                    }
                    const auto pred = s.predecessor(static_cast<Node16::index_t>(x));
                    return pred != s.end() ? std::make_optional<std::size_t>(*pred) : std::nullopt;
                },
                [&](const auto& s) -> std::optional<std::size_t> {
                    if (x == 0) {
                        return std::nullopt;
                    }
                    if (x >= s.universe_size()) {
                        return std::make_optional<std::size_t>(s.max());
                    }
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
                [&](const Node8& s) -> std::optional<std::size_t> {
                    return std::make_optional<std::size_t>(*s.min());
                },
                [&](const Node16& s) -> std::optional<std::size_t> {
                    return std::make_optional<std::size_t>(*s.min());
                },
                [&](const auto& s) -> std::optional<std::size_t> {
                    return std::make_optional<std::size_t>(s.min());
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
                [&](const Node8& s) -> std::optional<std::size_t> {
                    return std::make_optional(static_cast<std::size_t>(*s.max()));
                },
                [&](const Node16& s) -> std::optional<std::size_t> {
                    return std::make_optional(static_cast<std::size_t>(*s.max()));
                },
                [&](const auto& s) -> std::optional<std::size_t> {
                    return std::make_optional(static_cast<std::size_t>(s.max()));
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
        destroy_storage();
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
                [](const auto& s) { return s.size(); },
            },
            storage_);
    }

    /**
     * Count elements in the inclusive range [start, end].
     * This implementation leverages node-level size/count helpers so that
     * iteration happens at cluster granularity (Node8/Node16 etc.) instead
     * of over each element via repeated `successor` calls.
     */
    inline std::size_t count_range(std::size_t start, std::size_t end) const {
        if (start > end) {
            return 0;
        }
        return std::visit(
            overload{
                [](std::monostate) -> std::size_t { return 0; },
                [&](const Node8& n) -> std::size_t {
                    const auto minv{*n.min()};
                    const auto maxv{*n.max()};
                    if (start > maxv || end < minv) {
                        return 0;
                    }
                    end = std::min(end, n.universe_size() - 1);
                    const auto lo{std::max(static_cast<Node8::index_t>(start), minv)};
                    const auto hi{std::min(static_cast<Node8::index_t>(end), maxv)};
                    return n.count_range({ .lo = lo, .hi = hi });
                },
                [&](const Node16& n) -> std::size_t {
                    const auto minv{*n.min()};
                    const auto maxv{*n.max()};
                    if (start > maxv || end < minv) {
                        return 0;
                    }
                    end = std::min(end, n.universe_size() - 1);
                    const auto lo{std::max(static_cast<Node16::index_t>(start), minv)};
                    const auto hi{std::min(static_cast<Node16::index_t>(end), maxv)};
                    return n.count_range({ .lo = lo, .hi = hi });
                },
                [&](const auto& n) -> std::size_t {
                    using NodeType = std::decay_t<decltype(n)>;
                    const auto minv{n.min()};
                    const auto maxv{n.max()};
                    if (start > maxv || end < minv) {
                        return 0;
                    }
                    end = std::min(end, n.universe_size() - 1);
                    const auto lo{std::max(static_cast<index_t<NodeType>>(start), minv)};
                    const auto hi{std::min(static_cast<index_t<NodeType>>(end), maxv)};
                    return n.count_range({ .lo = lo, .hi = hi });
                },
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
        return {begin(), end()};
    }

    /**
     * @brief Gets memory usage statistics
     * @return Memory statistics structure
     */
    inline VebTreeMemoryStats get_memory_stats() const {
        return std::visit(
            overload{
                [](std::monostate) { return VebTreeMemoryStats{}; },
                [](const auto& s) { return s.get_memory_stats(); },
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
                [](const auto& s) { return s.universe_size(); },
            },
            storage_);
    }

    // helper to get tag for serialization. couldn't put inside VebCommon.hpp due to circular dependency
    template <typename S>
    static constexpr auto tag_v = [] {
        using node_t = std::remove_cvref_t<S>;
        if constexpr (std::is_same_v<node_t, std::monostate>) {
            return VebSerializeTag::NODE0;
        } else if constexpr (std::is_same_v<node_t, struct Node8>) {
            return VebSerializeTag::NODE8;
        } else if constexpr (std::is_same_v<node_t, struct Node16>) {
            return VebSerializeTag::NODE16;
        } else if constexpr (std::is_same_v<node_t, struct Node32>) {
            return VebSerializeTag::NODE32;
        } else if constexpr (std::is_same_v<node_t, struct Node64>) {
            return VebSerializeTag::NODE64;
        } else {
            static_assert(sizeof(S) == 0, "Unsupported type for tag_v");
        }
    }();
    inline std::string serialize() const {
        std::string out;
        // magic "vebbitset" (9 bytes)
        out.append("vebbitset", 9);
        // encver
        write_u8(out, 0);

        std::visit([&](const auto& n) {
            write_tag(out, tag_v<decltype(n)>);
            if constexpr (!std::is_same_v<std::remove_cvref_t<decltype(n)>, std::monostate>) {
                n.serialize(out);
            }
        }, storage_);

        return out;
    }

    static inline VebTree deserialize(std::string_view buf) {
        auto pos{0uz};
        if (buf.size() < 11) {
            throw std::runtime_error("buffer too small");
        }
        // verify magic
        const char *magic{"vebbitset"};
        for (auto i{0uz}; i < 9; ++i) {
            if (buf[pos++] != magic[i]) {
                throw std::runtime_error("magic mismatch");
            }
        }
        // encver
        const auto encver{read_u8(buf, pos)};
        if (encver != 0) {
            throw std::runtime_error("unsupported encver");
        }

        VebTree t{};
        const auto node_tag{read_tag(buf, pos)};
        switch (node_tag) {
        case VebSerializeTag::NODE0: {
            return t;
        }
        case VebSerializeTag::NODE8: {
            t.storage_ = Node8::deserialize(buf, pos);
            break;
        }
        case VebSerializeTag::NODE16: {
            t.storage_ = Node16::deserialize(buf, pos, 0, t.allocated_);
            break;
        }
        case VebSerializeTag::NODE32: {
            t.storage_ = Node32::deserialize(buf, pos, t.allocated_);
            break;
        }
        case VebSerializeTag::NODE64: {
            t.storage_ = Node64::deserialize(buf, pos, t.allocated_);
            break;
        }
        default: {
            throw std::runtime_error("deserialize: unsupported node_tag");
        }
        }
        t.max_seen_ = t.max().value();
        return t;
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
        auto min_val{min()};
        return Iterator{this, min_val.value_or(SIZE_MAX), !min_val.has_value()};
    }

    inline Iterator rbegin() const {
        auto max_val{max()};
        return Iterator{this, max_val.value_or(0uz), !max_val.has_value()};
    }

    /**
     * @brief Iterator to the end
     * @return Iterator to the end
     */
    inline Iterator end() const {
        return Iterator{this, SIZE_MAX, true};
    }

    inline Iterator rend() const {
        return Iterator{this, 0uz, true};
    }

    /**
     * @brief Set intersection operator
     * @param other The other VEB tree to intersect with
     * @return A new VEB tree containing elements present in both trees
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree operator&(const VebTree& other) const {
        VebTree result{*this};
        result &= other;
        return result;
    }

    /**
     * @brief Set union operator
     * @param other The other VEB tree to union with
     * @return A new VEB tree containing elements present in either tree
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree operator|(const VebTree& other) const {
        VebTree result{*this};
        result |= other;
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
        VebTree result{*this};
        result ^= other;
        return result;
    }

    /**
     * @brief In-place intersection operator
     * @param other The other VEB tree to intersect with
     * @return Reference to this tree after intersection
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree& operator&=(const VebTree& other) {
        std::visit(
            overload{
                [](std::monostate, std::monostate) -> void {
                },
                [](std::monostate, const Node8&) -> void {
                },
                [](std::monostate, const Node16&) -> void {
                },
                [](std::monostate, const Node32&) -> void {
                },
                [](std::monostate, const Node64&) -> void {
                },
                [&](Node8&, std::monostate) -> void {
                    storage_ = std::monostate{};
                },
                [&](Node16& a, std::monostate) -> void {
                    a.destroy(allocated_);
                    storage_ = std::monostate{};
                },
                [&](Node32& a, std::monostate) -> void {
                    a.destroy(allocated_);
                    storage_ = std::monostate{};
                },
                [&](Node64& a, std::monostate) -> void {
                    a.destroy(allocated_);
                    storage_ = std::monostate{};
                },
                [&](Node8& a, const Node8& b) -> void {
                    if (a.and_inplace(b)) {
                        storage_ = std::monostate{};
                    }
                },
                [&](Node16& a, const Node16& b) -> void {
                    if (a.and_inplace(b, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                },
                [&](Node32& a, const Node32& b) -> void {
                    if (a.and_inplace(b, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                },
                [&](Node64& a, const Node64& b) -> void {
                    if (a.and_inplace(b, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                },
                [&](Node8& a, const Node16& b) -> void {
                    auto a16{Node16::new_from_node8(a, allocated_)};
                    if (a16.and_inplace(b, allocated_)) {
                        a16.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a16);
                    }
                },
                [&](Node16& a, const Node8& b) -> void {
                    auto b16{Node16::new_from_node8(b, allocated_)};
                    if (a.and_inplace(b16, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b16.destroy(allocated_);
                },
                [&](Node8& a, const Node32& b) -> void {
                    auto a32{Node32::new_from_node8(a, allocated_)};
                    if (a32.and_inplace(b, allocated_)) {
                        a32.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a32);
                    }
                },
                [&](Node32& a, const Node8& b) -> void {
                    auto b32{Node32::new_from_node8(b, allocated_)};
                    if (a.and_inplace(b32, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b32.destroy(allocated_);
                },
                [&](Node16& a, const Node32& b) -> void {
                    auto a32{Node32::new_from_node16(std::move(a), allocated_)};
                    if (a32.and_inplace(b, allocated_)) {
                        a32.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a32);
                    }
                },
                [&](Node32& a, const Node16& b) -> void {
                    auto b32{Node32::new_from_node16(b.clone(allocated_), allocated_)};
                    if (a.and_inplace(b32, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b32.destroy(allocated_);
                },
                [&](Node8& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node8(a, allocated_)};
                    if (a64.and_inplace(b, allocated_)) {
                        a64.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a64);
                    }
                },
                [&](Node64& a, const Node8& b) -> void {
                    auto b64{Node64::new_from_node8(b, allocated_)};
                    if (a.and_inplace(b64, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b64.destroy(allocated_);
                },
                [&](Node16& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node16(std::move(a), allocated_)};
                    if (a64.and_inplace(b, allocated_)) {
                        a64.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a64);
                    }
                },
                [&](Node64& a, const Node16& b) -> void {
                    auto b64{Node64::new_from_node16(b.clone(allocated_), allocated_)};
                    if (a.and_inplace(b64, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b64.destroy(allocated_);
                },
                [&](Node32& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node32(std::move(a), allocated_)};
                    if (a64.and_inplace(b, allocated_)) {
                        a64.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a64);
                    }
                },
                [&](Node64& a, const Node32& b) -> void {
                    auto b64{Node64::new_from_node32(b.clone(allocated_), allocated_)};
                    if (a.and_inplace(b64, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b64.destroy(allocated_);
                },
                [](auto&&, auto&&) -> void {
                    std::unreachable();
                }
            },
            storage_, other.storage_
        );

        return *this;
    }

    /**
     * @brief In-place union operator
     * @param other The other VEB tree to union with
     * @return Reference to this tree after union
     *
     * Time complexity: O(log log U) per node
     */
    inline VebTree& operator|=(const VebTree& other) {
        if (other.empty()) {
            return *this;
        }
        if (empty()) {
            storage_ = std::visit(overload{
                [](std::monostate) -> StorageType { return std::monostate{}; },
                [](const Node8& node) -> StorageType { return node; },
                [&](const auto& node) -> StorageType { return node.clone(allocated_); },
            }, other.storage_);
            return *this;
        }

        std::visit(
            overload{
                [](Node8& a, const Node8& b) -> void {
                    a.or_inplace(b);
                },
                [&](Node16& a, const Node16& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node32& a, const Node32& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node64& a, const Node64& b) -> void {
                    a.or_inplace(b, allocated_);
                },
                [&](Node8& a, const Node16& b) -> void {
                    auto a16{Node16::new_from_node8(a, allocated_)};
                    a16.or_inplace(b, allocated_);
                    storage_ = std::move(a16);
                },
                [&](Node16& a, const Node8& b) -> void {
                    auto b16{Node16::new_from_node8(b, allocated_)};
                    a.or_inplace(b16, allocated_);
                    b16.destroy(allocated_);
                },
                [&](Node8& a, const Node32& b) -> void {
                    auto a32{Node32::new_from_node8(a, allocated_)};
                    a32.or_inplace(b, allocated_);
                    storage_ = std::move(a32);
                },
                [&](Node32& a, const Node8& b) -> void {
                    auto b32{Node32::new_from_node8(b, allocated_)};
                    a.or_inplace(b32, allocated_);
                    b32.destroy(allocated_);
                },
                [&](Node16& a, const Node32& b) -> void {
                    auto a32{Node32::new_from_node16(std::move(a), allocated_)};
                    a32.or_inplace(b, allocated_);
                    storage_ = std::move(a32);
                },
                [&](Node32& a, const Node16& b) -> void {
                    auto b32{Node32::new_from_node16(b.clone(allocated_), allocated_)};
                    a.or_inplace(b32, allocated_);
                    b32.destroy(allocated_);
                },
                [&](Node8& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node8(a, allocated_)};
                    a64.or_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node8& b) -> void {
                    auto b64{Node64::new_from_node8(b, allocated_)};
                    a.or_inplace(b64, allocated_);
                    b64.destroy(allocated_);
                },
                [&](Node16& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node16(std::move(a), allocated_)};
                    a64.or_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node16& b) -> void {
                    auto b64{Node64::new_from_node16(b.clone(allocated_), allocated_)};
                    a.or_inplace(b64, allocated_);
                    b64.destroy(allocated_);
                },
                [&](Node32& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node32(std::move(a), allocated_)};
                    a64.or_inplace(b, allocated_);
                    storage_ = std::move(a64);
                },
                [&](Node64& a, const Node32& b) -> void {
                    auto b64{Node64::new_from_node32(b.clone(allocated_), allocated_)};
                    a.or_inplace(b64, allocated_);
                    b64.destroy(allocated_);
                },
                [](auto&&, auto&&) {
                    std::unreachable();
                }
            },
            storage_, other.storage_
        );

        return *this;
    }

    /**
     * @brief In-place symmetric difference (XOR) operator
     * @param other The other VEB tree to XOR with
     * @return Reference to this tree after XOR
     */
    inline VebTree& operator^=(const VebTree& other) {
        if (other.empty()) {
            return *this;
        }
        if (empty()) {
            storage_ = std::visit(overload{
                [](std::monostate) -> StorageType { return std::monostate{}; },
                [](const Node8& node) -> StorageType { return node; },
                [&](const auto& node) -> StorageType { return node.clone(allocated_); },
            }, other.storage_);
            return *this;
        }
        std::visit(
            overload{
                [&](Node8& a, const Node8& b) -> void {
                    if (a.xor_inplace(b)) {
                        storage_ = std::monostate{};
                    }
                },
                [&](Node16& a, const Node16& b) -> void {
                    if (a.xor_inplace(b, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                },
                [&](Node32& a, const Node32& b) -> void {
                    if (a.xor_inplace(b, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                },
                [&](Node64& a, const Node64& b) -> void {
                    if (a.xor_inplace(b, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                },
                [&](Node8& a, const Node16& b) -> void {
                    auto a16{Node16::new_from_node8(a, allocated_)};
                    if (a16.xor_inplace(b, allocated_)) {
                        a16.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a16);
                    }
                },
                [&](Node16& a, const Node8& b) -> void {
                    auto b16{Node16::new_from_node8(b, allocated_)};
                    if (a.xor_inplace(b16, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b16.destroy(allocated_);
                },
                [&](Node8& a, const Node32& b) -> void {
                    auto a32{Node32::new_from_node8(a, allocated_)};
                    if (a32.xor_inplace(b, allocated_)) {
                        a32.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a32);
                    }
                },
                [&](Node32& a, const Node8& b) -> void {
                    auto b32{Node32::new_from_node8(b, allocated_)};
                    if (a.xor_inplace(b32, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b32.destroy(allocated_);
                },
                [&](Node16& a, const Node32& b) -> void {
                    auto a32{Node32::new_from_node16(std::move(a), allocated_)};
                    if (a32.xor_inplace(b, allocated_)) {
                        a32.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a32);
                    }
                },
                [&](Node32& a, const Node16& b) -> void {
                    auto b32{Node32::new_from_node16(b.clone(allocated_), allocated_)};
                    if (a.xor_inplace(b32, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b32.destroy(allocated_);
                },
                [&](Node8& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node8(a, allocated_)};
                    if (a64.xor_inplace(b, allocated_)) {
                        a64.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a64);
                    }
                },
                [&](Node64& a, const Node8& b) -> void {
                    auto b64{Node64::new_from_node8(b, allocated_)};
                    if (a.xor_inplace(b64, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b64.destroy(allocated_);
                },
                [&](Node16& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node16(std::move(a), allocated_)};
                    if (a64.xor_inplace(b, allocated_)) {
                        a64.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a64);
                    }
                },
                [&](Node64& a, const Node16& b) -> void {
                    auto b64{Node64::new_from_node16(b.clone(allocated_), allocated_)};
                    if (a.xor_inplace(b64, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b64.destroy(allocated_);
                },
                [&](Node32& a, const Node64& b) -> void {
                    auto a64{Node64::new_from_node32(std::move(a), allocated_)};
                    if (a64.xor_inplace(b, allocated_)) {
                        a64.destroy(allocated_);
                        storage_ = std::monostate{};
                    } else {
                        storage_ = std::move(a64);
                    }
                },
                [&](Node64& a, const Node32& b) -> void {
                    auto b64{Node64::new_from_node32(b.clone(allocated_), allocated_)};
                    if (a.xor_inplace(b64, allocated_)) {
                        a.destroy(allocated_);
                        storage_ = std::monostate{};
                    }
                    b64.destroy(allocated_);
                },
                [](auto&&, auto&&) -> void {
                    std::unreachable();
                }
            },
            storage_, other.storage_
        );

        return *this;
    }

    /**
     * @brief Equality comparison operator
     */
    inline bool operator==(const VebTree& other) const {
        if (size() != other.size()) {
            return false;
        }
        for (std::size_t element : *this) {
            if (!other.contains(element)) {
                return false;
            }
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

#endif // VEBTREE_HPP
