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

static VebTree_OptionalSize_t to_c_optional(std::optional<std::size_t> opt) {
    if (opt.has_value()) {
        return {true, opt.value()};
    } else {
        return {false, 0};
    }
}

VebTree_Handle_t vebtree_create() {
    VebTree_Handle_t p = static_cast<VebTree_Handle_t>(malloc(sizeof *p));
    std::construct_at(p);
    return p;
}

static void vebtree_insert(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    handle->insert(x);
}

static void vebtree_remove(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    handle->remove(x);
}

static bool vebtree_contains(const_VebTree_Handle_t handle, size_t x) {
    assert(handle);
    return handle->contains(x);
}

static VebTree_OptionalSize_t vebtree_successor(const_VebTree_Handle_t handle, size_t x) {
    assert(handle);
    auto result{handle->successor(x)};
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_predecessor(const_VebTree_Handle_t handle, size_t x) {
    assert(handle);
    auto result{handle->predecessor(x)};
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_min(const_VebTree_Handle_t handle) {
    assert(handle);
    auto result{handle->min()};
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_max(const_VebTree_Handle_t handle) {
    assert(handle);
    auto result{handle->max()};
    return to_c_optional(result);
}

static bool vebtree_empty(const_VebTree_Handle_t handle) {
    assert(handle);
    return handle->empty();
}

static void vebtree_clear(VebTree_Handle_t handle) {
    assert(handle);
    handle->clear();
}

static std::size_t vebtree_size(const_VebTree_Handle_t handle) {
    assert(handle);
    return handle->size();
}

static std::size_t* vebtree_to_array(const_VebTree_Handle_t handle) {
    assert(handle);
    std::size_t* array = static_cast<std::size_t*>(malloc(handle->size() * sizeof *array));
    for (std::size_t i = 0; auto v : *handle) {
        array[i++] = v;
    }
    return array;
}

static VebTree_MemoryStats_t vebtree_get_memory_stats(const_VebTree_Handle_t handle) {
    assert(handle);
    auto cpp_stats{handle->get_memory_stats()};
    return VebTree_MemoryStats_t{          
        .total_clusters = cpp_stats.total_clusters,
        .max_depth = cpp_stats.max_depth,  
        .total_nodes = cpp_stats.total_nodes,
    };
}

static std::size_t vebtree_get_allocated_memory(const_VebTree_Handle_t handle) {
    assert(handle);
    return handle->get_allocated_bytes();
}

static std::size_t vebtree_universe_size(const_VebTree_Handle_t handle) {
    assert(handle);
    return handle->universe_size();
}

static bool vebtree_equals(const_VebTree_Handle_t handle1, const_VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    return *handle1 == *handle2;
}

static void vebtree_intersection(VebTree_Handle_t handle1, const_VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1 &= *handle2;
}

static void vebtree_union_op(VebTree_Handle_t handle1, const_VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1 |= *handle2;
}

static void vebtree_symmetric_difference(VebTree_Handle_t handle1, const_VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1 ^= *handle2;
}

static void vebtree_move(VebTree_Handle_t dst, VebTree_Handle_t src) {
    assert(dst && src);
    *dst = std::move(*src);
}

static void vebtree_destroy(VebTree_Handle_t handle) {
    if (handle != nullptr) {
        handle->~VebTree();
        free(handle);
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
    .move = vebtree_move,
    .destroy = vebtree_destroy,
};

const VebTree_API_t* vebtree_get_api() {
    return &vebtree_api;
}
