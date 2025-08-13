/**
 * @file VebTree_c_api.cpp
 * @brief C API implementation for van Emde Boas Tree
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
    VebTree* veb_ptr = nullptr;

    explicit VebTree_Handle() : veb_ptr{nullptr} {
        veb_ptr = static_cast<VebTree*>(std::malloc(sizeof *veb_ptr));
        new (veb_ptr) VebTree{};
    }

    ~VebTree_Handle() {
        if (veb_ptr != nullptr) {
            veb_ptr->~VebTree();
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

VebTree_Handle_t vebtree_create() {
    auto ptr = static_cast<VebTree_Handle_t>(std::malloc(sizeof(VebTree_Handle)));
    new (ptr) VebTree_Handle{};
    return ptr;
}

static void vebtree_insert(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    handle->veb_ptr->insert(x);
}

static void vebtree_remove(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    handle->veb_ptr->remove(x);
}

static bool vebtree_contains(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    return handle->veb_ptr->contains(x);
}

static VebTree_OptionalSize_t vebtree_successor(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    auto result = handle->veb_ptr->successor(x);
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_predecessor(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    auto result = handle->veb_ptr->predecessor(x);
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_min(VebTree_Handle_t handle) {
    assert(handle);
    auto result = handle->veb_ptr->min();
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_max(VebTree_Handle_t handle) {
    assert(handle);
    auto result = handle->veb_ptr->max();
    return to_c_optional(result);
}

static bool vebtree_empty(VebTree_Handle_t handle) {
    assert(handle);
    return handle->veb_ptr->empty();
}

static void vebtree_clear(VebTree_Handle_t handle) {
    assert(handle);
    handle->veb_ptr->clear();
}

static std::size_t vebtree_size(VebTree_Handle_t handle) {
    assert(handle);
    return handle->veb_ptr->size();
}

static std::size_t* vebtree_to_array(VebTree_Handle_t handle) {
    assert(handle);
    auto vec = handle->veb_ptr->to_vector();
    std::size_t* array = static_cast<std::size_t*>(malloc(vec.size() * sizeof(std::size_t)));
    std::ranges::copy(vec, array);
    return array;
}

static VebTree_MemoryStats_t vebtree_get_memory_stats(VebTree_Handle_t handle) {
    assert(handle);
    auto cpp_stats = handle->veb_ptr->get_memory_stats();
    return VebTree_MemoryStats_t{          
        .total_clusters = cpp_stats.total_clusters,
        .max_depth = cpp_stats.max_depth,  
        .total_nodes = cpp_stats.total_nodes,
    };
}

static std::size_t vebtree_get_allocated_memory(VebTree_Handle_t handle) {
    assert(handle);
    return handle->veb_ptr->get_allocated_bytes();
}

static std::size_t vebtree_universe_size(VebTree_Handle_t handle) {
    assert(handle);
    return handle->veb_ptr->universe_size();
}

static bool vebtree_equals(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    return *handle1->veb_ptr == *handle2->veb_ptr;
}

static void vebtree_intersection(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1->veb_ptr &= *handle2->veb_ptr;
}

static void vebtree_union_op(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1->veb_ptr |= *handle2->veb_ptr;
}

static void vebtree_symmetric_difference(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1->veb_ptr ^= *handle2->veb_ptr;
}

static void vebtree_destroy(VebTree_Handle_t handle) {
    if (handle) {                          
        handle->~VebTree_Handle();
        std::free(handle);
    }
}

static constexpr VebTree_API_t vebtree_api = {
    .insert = vebtree_insert,
    .remove = vebtree_remove,
    .contains = vebtree_contains,
    .successor = vebtree_successor,
    .predecessor = vebtree_predecessor,
    .min = vebtree_min,
    .max = vebtree_max,
    .empty = vebtree_empty,
    .clear = vebtree_clear,
    .size = vebtree_size,
    .to_array = vebtree_to_array,
    .get_memory_stats = vebtree_get_memory_stats,
    .get_allocated_memory = vebtree_get_allocated_memory,
    .universe_size = vebtree_universe_size,
    .equals = vebtree_equals,
    .intersection = vebtree_intersection,
    .union_op = vebtree_union_op,
    .symmetric_difference = vebtree_symmetric_difference,
    .destroy = vebtree_destroy,
};

const VebTree_API_t* vebtree_get_api() {
    return &vebtree_api;
}
