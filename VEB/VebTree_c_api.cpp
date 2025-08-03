/**
 * @file VebTree_c_api.cpp
 * @brief C API implementation for van Emde Boas Tree
 *
 * This file implements the C API defined in VebTree.h using X macros
 * to generate implementations for different hash table backends.
 */

#include <cstdlib>
#include <iostream>
#include <memory>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cassert>
#include <ranges>

#include "VebTree.h"
#include "VebTree.hpp"

struct VebTree_Handle {
    VebTree_ImplType_t impl_type;
    void* veb_ptr = nullptr;

    explicit VebTree_Handle(VebTree_ImplType_t type) : impl_type{type} {
        switch (impl_type) {
        case VEBTREE_STD:
            veb_ptr = std::malloc(sizeof(StdVebTree));
            new (veb_ptr) StdVebTree{};
            break;
#ifdef HAVE_ABSL
        case VEBTREE_ABSL:
            veb_ptr = std::malloc(sizeof(AbslVebTree));
            new (veb_ptr) AbslVebTree{};
            break;
#endif
#ifdef HAVE_BOOST
        case VEBTREE_BOOST_FLAT:
            veb_ptr = std::malloc(sizeof(BoostFlatVebTree));
            new (veb_ptr) BoostFlatVebTree{};
            break;
        case VEBTREE_BOOST_NODE:
            veb_ptr = std::malloc(sizeof(BoostNodeVebTree));
            new (veb_ptr) BoostNodeVebTree{};
            break;
        case VEBTREE_BOOST:
            veb_ptr = std::malloc(sizeof(BoostVebTree));
            new (veb_ptr) BoostVebTree{};
            break;
#endif
        default:
            std::unreachable();
        }
    }

    ~VebTree_Handle() {
        if (veb_ptr != nullptr) {
            switch (impl_type) {
            case VEBTREE_STD:
                static_cast<StdVebTree*>(veb_ptr)->~StdVebTree();
                break;
#ifdef HAVE_ABSL
            case VEBTREE_ABSL:
                static_cast<AbslVebTree*>(veb_ptr)->~AbslVebTree();
                break;
#endif
#ifdef HAVE_BOOST
            case VEBTREE_BOOST_FLAT:
                static_cast<BoostFlatVebTree*>(veb_ptr)->~BoostFlatVebTree();
                break;
            case VEBTREE_BOOST_NODE:
                static_cast<BoostNodeVebTree*>(veb_ptr)->~BoostNodeVebTree();
                break;
            case VEBTREE_BOOST:
                static_cast<BoostVebTree*>(veb_ptr)->~BoostVebTree();
                break;
#endif
            default:
                std::unreachable();
            }
            std::free(veb_ptr);
            veb_ptr = nullptr;
        }
    }
    VebTree_Handle(const VebTree_Handle&) = delete;
    VebTree_Handle(VebTree_Handle&&) = delete;
    VebTree_Handle& operator=(const VebTree_Handle&) = delete;
    VebTree_Handle& operator=(VebTree_Handle&&) = delete;
};

static VebTree_OptionalSize_t to_c_optional(std::optional<std::size_t> opt) {
    if (opt.has_value()) {
        return {true, opt.value()};
    } else {
        return {false, 0};
    }
}

#define VEB_TREE(VEB_TYPE, handle) (*static_cast<VEB_TYPE*>(handle->veb_ptr))

#define VEBTREE_IMPL(PREFIX, VEB_TYPE, IMPL_TYPE)                                                              \
    static VebTree_Handle_t PREFIX##_create() {                                                                \
        auto ptr = static_cast<VebTree_Handle_t>(std::malloc(sizeof(VebTree_Handle)));                         \
        new (ptr) VebTree_Handle{IMPL_TYPE};                                                                   \
        return ptr;                                                                                            \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_insert(VebTree_Handle_t handle, size_t x) {                                           \
        assert(handle);                                                                                        \
        VEB_TREE(VEB_TYPE, handle).insert(x);                                                                  \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_remove(VebTree_Handle_t handle, size_t x) {                                           \
        assert(handle);                                                                                        \
        VEB_TREE(VEB_TYPE, handle).remove(x);                                                                  \
    }                                                                                                          \
                                                                                                               \
    static bool PREFIX##_contains(VebTree_Handle_t handle, size_t x) {                                         \
        assert(handle);                                                                                        \
        return VEB_TREE(VEB_TYPE, handle).contains(x);                                                         \
    }                                                                                                          \
                                                                                                               \
    static VebTree_OptionalSize_t PREFIX##_successor(VebTree_Handle_t handle, size_t x) {                      \
        assert(handle);                                                                                        \
        auto result = VEB_TREE(VEB_TYPE, handle).successor(x);                                                 \
        return to_c_optional(result);                                                                          \
    }                                                                                                          \
                                                                                                               \
    static VebTree_OptionalSize_t PREFIX##_predecessor(VebTree_Handle_t handle, size_t x) {                    \
        assert(handle);                                                                                        \
        auto result = VEB_TREE(VEB_TYPE, handle).predecessor(x);                                               \
        return to_c_optional(result);                                                                          \
    }                                                                                                          \
                                                                                                               \
    static VebTree_OptionalSize_t PREFIX##_min(VebTree_Handle_t handle) {                                      \
        assert(handle);                                                                                        \
        auto result = VEB_TREE(VEB_TYPE, handle).min();                                                        \
        return to_c_optional(result);                                                                          \
    }                                                                                                          \
                                                                                                               \
    static VebTree_OptionalSize_t PREFIX##_max(VebTree_Handle_t handle) {                                      \
        assert(handle);                                                                                        \
        auto result = VEB_TREE(VEB_TYPE, handle).max();                                                        \
        return to_c_optional(result);                                                                          \
    }                                                                                                          \
                                                                                                               \
    static bool PREFIX##_empty(VebTree_Handle_t handle) {                                                      \
        assert(handle);                                                                                        \
        return VEB_TREE(VEB_TYPE, handle).empty();                                                             \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_clear(VebTree_Handle_t handle) {                                                      \
        assert(handle);                                                                                        \
        VEB_TREE(VEB_TYPE, handle).clear();                                                                    \
    }                                                                                                          \
                                                                                                               \
    static std::size_t PREFIX##_size(VebTree_Handle_t handle) {                                                \
        assert(handle);                                                                                        \
        return VEB_TREE(VEB_TYPE, handle).size();                                                              \
    }                                                                                                          \
                                                                                                               \
    static std::size_t* PREFIX##_to_array(VebTree_Handle_t handle) {                                           \
        assert(handle);                                                                                        \
        auto vec = VEB_TREE(VEB_TYPE, handle).to_vector();                                                     \
        std::size_t* array = static_cast<std::size_t*>(malloc(vec.size() * sizeof(std::size_t)));              \
        std::ranges::copy(vec, array);                                                                         \
        return array;                                                                                          \
    }                                                                                                          \
                                                                                                               \
    static VebTree_MemoryStats_t PREFIX##_get_memory_stats(VebTree_Handle_t handle) {                          \
        assert(handle);                                                                                        \
        auto cpp_stats = VEB_TREE(VEB_TYPE, handle).get_memory_stats();                                        \
        return VebTree_MemoryStats_t{                                                                          \
            .total_clusters = cpp_stats.total_clusters,                                                        \
            .max_depth = cpp_stats.max_depth,                                                                  \
            .total_nodes = cpp_stats.total_nodes,                                                              \
        };                                                                                                     \
    }                                                                                                          \
                                                                                                               \
    static std::size_t PREFIX##_get_allocated_memory(VebTree_Handle_t handle) {                                \
        assert(handle);                                                                                        \
        return VEB_TREE(VEB_TYPE, handle).get_allocated_bytes();                                               \
    }                                                                                                          \
                                                                                                               \
    static std::size_t PREFIX##_universe_size(VebTree_Handle_t handle) {                                       \
        assert(handle);                                                                                        \
        return VEB_TREE(VEB_TYPE, handle).universe_size();                                                     \
    }                                                                                                          \
                                                                                                               \
    static bool PREFIX##_equals(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {                          \
        assert(handle1 && handle2);                                                                            \
        assert(handle1->impl_type == handle2->impl_type);                                                      \
        return VEB_TREE(VEB_TYPE, handle1) == VEB_TREE(VEB_TYPE, handle2);                                     \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_intersection(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {                    \
        assert(handle1 && handle2);                                                                            \
        assert(handle1->impl_type == handle2->impl_type);                                                      \
        VEB_TREE(VEB_TYPE, handle1) &= VEB_TREE(VEB_TYPE, handle2);                                            \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_union_op(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {                        \
        assert(handle1 && handle2);                                                                            \
        assert(handle1->impl_type == handle2->impl_type);                                                      \
        VEB_TREE(VEB_TYPE, handle1) |= VEB_TREE(VEB_TYPE, handle2);                                            \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_symmetric_difference(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {            \
        assert(handle1 && handle2);                                                                            \
        assert(handle1->impl_type == handle2->impl_type);                                                      \
        VEB_TREE(VEB_TYPE, handle1) ^= VEB_TREE(VEB_TYPE, handle2);                                            \
    }                                                                                                          \
                                                                                                               \
    static const char* PREFIX##_hash_table_name() {                                                            \
        return VEB_TYPE::hash_table_name();                                                                    \
    }                                                                                                          \
                                                                                                               \
    static void PREFIX##_destroy(VebTree_Handle_t handle) {                                                    \
        if (handle) {                                                                                          \
            handle->~VebTree_Handle();                                                                         \
            std::free(handle);                                                                                 \
        }                                                                                                      \
    }

VEBTREE_IMPL(vebtree_std, StdVebTree, VEBTREE_STD)

#ifdef HAVE_ABSL
VEBTREE_IMPL(vebtree_absl, AbslVebTree, VEBTREE_ABSL)
#endif

#ifdef HAVE_BOOST
VEBTREE_IMPL(vebtree_boost_flat, BoostFlatVebTree, VEBTREE_BOOST_FLAT)
VEBTREE_IMPL(vebtree_boost_node, BoostNodeVebTree, VEBTREE_BOOST_NODE)
VEBTREE_IMPL(vebtree_boost, BoostVebTree, VEBTREE_BOOST)
#endif

VebTree_Handle_t vebtree_create(VebTree_ImplType_t impl_type) {
    assert(impl_type >= VEBTREE_STD && impl_type < VEBTREE_NUM_IMPL_TYPES);

    switch (impl_type) {
    case VEBTREE_STD: return vebtree_std_create();
#ifdef HAVE_ABSL
    case VEBTREE_ABSL: return vebtree_absl_create();
#endif
#ifdef HAVE_BOOST
    case VEBTREE_BOOST_FLAT: return vebtree_boost_flat_create();
    case VEBTREE_BOOST_NODE: return vebtree_boost_node_create();
    case VEBTREE_BOOST: return vebtree_boost_create();
#endif
    default: return nullptr;
    }
}

#define VEBTREE_API_TABLE(PREFIX)                               \
    static constexpr VebTree_API_t PREFIX##_api = {             \
        .insert = PREFIX##_insert,                              \
        .remove = PREFIX##_remove,                              \
        .contains = PREFIX##_contains,                          \
        .successor = PREFIX##_successor,                        \
        .predecessor = PREFIX##_predecessor,                    \
        .min = PREFIX##_min,                                    \
        .max = PREFIX##_max,                                    \
        .empty = PREFIX##_empty,                                \
        .clear = PREFIX##_clear,                                \
        .size = PREFIX##_size,                                  \
        .to_array = PREFIX##_to_array,                          \
        .get_memory_stats = PREFIX##_get_memory_stats,          \
        .get_allocated_memory = PREFIX##_get_allocated_memory,  \
        .universe_size = PREFIX##_universe_size,                \
        .hash_table_name = PREFIX##_hash_table_name,            \
        .equals = PREFIX##_equals,                              \
        .intersection = PREFIX##_intersection,                  \
        .union_op = PREFIX##_union_op,                          \
        .symmetric_difference = PREFIX##_symmetric_difference,  \
        .destroy = PREFIX##_destroy,                            \
    };

VEBTREE_API_TABLE(vebtree_std)

#ifdef HAVE_ABSL
VEBTREE_API_TABLE(vebtree_absl)
#endif

#ifdef HAVE_BOOST
VEBTREE_API_TABLE(vebtree_boost_flat)
VEBTREE_API_TABLE(vebtree_boost_node)
VEBTREE_API_TABLE(vebtree_boost)
#endif

VebTree_ImplType_t vebtree_get_impl_type(VebTree_Handle_t handle) {
    assert(handle);
    return handle->impl_type;
}

const VebTree_API_t* vebtree_get_api(VebTree_Handle_t handle) {
    assert(handle);
    switch (handle->impl_type) {
    case VEBTREE_STD: return &vebtree_std_api;
#ifdef HAVE_ABSL
    case VEBTREE_ABSL: return &vebtree_absl_api;
#endif
#ifdef HAVE_BOOST
    case VEBTREE_BOOST_FLAT: return &vebtree_boost_flat_api;
    case VEBTREE_BOOST_NODE: return &vebtree_boost_node_api;
    case VEBTREE_BOOST: return &vebtree_boost_api;
#endif
    default: std::unreachable();
    }
}
