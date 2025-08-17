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
    VebTree_Handle_t p = static_cast<VebTree_Handle_t>(std::malloc(sizeof *p));
    new (p) VebTree;
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

static bool vebtree_contains(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    return handle->contains(x);
}

static VebTree_OptionalSize_t vebtree_successor(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    auto result = handle->successor(x);
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_predecessor(VebTree_Handle_t handle, size_t x) {
    assert(handle);
    auto result = handle->predecessor(x);
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_min(VebTree_Handle_t handle) {
    assert(handle);
    auto result = handle->min();
    return to_c_optional(result);
}

static VebTree_OptionalSize_t vebtree_max(VebTree_Handle_t handle) {
    assert(handle);
    auto result = handle->max();
    return to_c_optional(result);
}

static bool vebtree_empty(VebTree_Handle_t handle) {
    assert(handle);
    return handle->empty();
}

static void vebtree_clear(VebTree_Handle_t handle) {
    assert(handle);
    handle->clear();
}

static std::size_t vebtree_size(VebTree_Handle_t handle) {
    assert(handle);
    return handle->size();
}

static std::size_t* vebtree_to_array(VebTree_Handle_t handle) {
    assert(handle);
    auto vec = handle->to_vector();
    std::size_t* array = static_cast<std::size_t*>(malloc(vec.size() * sizeof(std::size_t)));
    std::ranges::copy(vec, array);
    return array;
}

static VebTree_MemoryStats_t vebtree_get_memory_stats(VebTree_Handle_t handle) {
    assert(handle);
    auto cpp_stats = handle->get_memory_stats();
    return VebTree_MemoryStats_t{          
        .total_clusters = cpp_stats.total_clusters,
        .max_depth = cpp_stats.max_depth,  
        .total_nodes = cpp_stats.total_nodes,
    };
}

static std::size_t vebtree_get_allocated_memory(VebTree_Handle_t handle) {
    assert(handle);
    return handle->get_allocated_bytes();
}

static std::size_t vebtree_universe_size(VebTree_Handle_t handle) {
    assert(handle);
    return handle->universe_size();
}

static bool vebtree_equals(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    return *handle1 == *handle2;
}

static void vebtree_intersection(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1 &= *handle2;
}

static void vebtree_union_op(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1 |= *handle2;
}

static void vebtree_symmetric_difference(VebTree_Handle_t handle1, VebTree_Handle_t handle2) {
    assert(handle1 && handle2);
    *handle1 ^= *handle2;
}

static void vebtree_destroy(VebTree_Handle_t handle) {
    if (handle) {                          
        handle->~VebTree();
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
