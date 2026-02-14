/**
 * @file vebitset.cpp
 * @brief C API implementation for vEBitset (van Emde Boas Tree)
 */

#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string_view>
#include <string>

#include "vebitset.h"
#include "VEB/VebTree.hpp"

static vebitset_optional_t to_c_optional(std::optional<std::size_t> opt) {
    if (opt.has_value()) {
        return {true, opt.value()};
    } else {
        return {false, 0};
    }
}

vebitset_t vebitset_create() {
    vebitset_t p = static_cast<vebitset_t>(malloc(sizeof(VebTree)));
    if (p != nullptr) {
        std::construct_at(p);
    }
    return p;
}

void vebitset_destroy(vebitset_t handle) {
    if (handle != nullptr) {
        handle->~VebTree();
        free(handle);
    }
}

void vebitset_insert(vebitset_t handle, size_t x) {
    assert(handle != nullptr);
    handle->insert(x);
}

void vebitset_remove(vebitset_t handle, size_t x) {
    assert(handle != nullptr);
    handle->remove(x);
}

bool vebitset_contains(const_vebitset_t handle, size_t x) {
    assert(handle != nullptr);
    return handle->contains(x);
}

vebitset_optional_t vebitset_successor(const_vebitset_t handle, size_t x) {
    assert(handle != nullptr);
    return to_c_optional(handle->successor(x));
}

vebitset_optional_t vebitset_predecessor(const_vebitset_t handle, size_t x) {
    assert(handle != nullptr);
    return to_c_optional(handle->predecessor(x));
}

vebitset_optional_t vebitset_min(const_vebitset_t handle) {
    assert(handle != nullptr);
    return to_c_optional(handle->min());
}

vebitset_optional_t vebitset_max(const_vebitset_t handle) {
    assert(handle != nullptr);
    return to_c_optional(handle->max());
}

bool vebitset_is_empty(const_vebitset_t handle) {
    assert(handle != nullptr);
    return handle->empty();
}

void vebitset_clear(vebitset_t handle) {
    assert(handle != nullptr);
    handle->clear();
}

size_t vebitset_count(const_vebitset_t handle) {
    assert(handle != nullptr);
    return handle->size();
}

size_t vebitset_count_range(const_vebitset_t handle, size_t start, size_t end) {
    assert(handle != nullptr);
    return handle->count_range(start, end);
}

size_t* vebitset_to_array(const_vebitset_t handle, size_t *out_len) {
    assert(handle != nullptr);
    assert(out_len != nullptr);
    std::size_t len = handle->size();
    *out_len = len;
    if (len == 0) {
        return nullptr;
    }
    std::size_t* array = static_cast<std::size_t*>(malloc(len * sizeof(std::size_t)));
    if (array != nullptr) {
        std::size_t i = 0;
        for (auto v : *handle) {
            array[i++] = v;
        }
    }
    return array;
}

vebitset_stats_t vebitset_get_stats(const_vebitset_t handle) {
    assert(handle != nullptr);
    auto cpp_stats = handle->get_memory_stats();
    return vebitset_stats_t{          
        .total_clusters = cpp_stats.total_clusters,
        .max_depth = cpp_stats.max_depth,  
        .total_nodes = cpp_stats.total_nodes,
    };
}

size_t vebitset_allocated_memory(const_vebitset_t handle) {
    assert(handle != nullptr);
    return handle->get_allocated_bytes();
}

size_t vebitset_universe_size(const_vebitset_t handle) {
    assert(handle != nullptr);
    return handle->universe_size();
}

bool vebitset_equals(const_vebitset_t handle1, const_vebitset_t handle2) {
    assert(handle1 != nullptr && handle2 != nullptr);
    return *handle1 == *handle2;
}

void vebitset_intersection(vebitset_t handle1, const_vebitset_t handle2) {
    assert(handle1 != nullptr && handle2 != nullptr);
    *handle1 &= *handle2;
}

void vebitset_union(vebitset_t handle1, const_vebitset_t handle2) {
    assert(handle1 != nullptr && handle2 != nullptr);
    *handle1 |= *handle2;
}

void vebitset_symmetric_difference(vebitset_t handle1, const_vebitset_t handle2) {
    assert(handle1 != nullptr && handle2 != nullptr);
    *handle1 ^= *handle2;
}

void vebitset_move(vebitset_t dst, vebitset_t src) {
    assert(dst != nullptr && src != nullptr);
    *dst = std::move(*src);
}

const char* vebitset_serialize(vebitset_t handle, size_t *out_len) {
    assert(handle != nullptr);
    assert(out_len != nullptr);
    std::string buf = handle->serialize();
    *out_len = buf.size();
    char *out = static_cast<char*>(malloc(*out_len));
    if (out != nullptr) {
        std::memcpy(out, buf.data(), *out_len);
    }
    return out;
}

vebitset_t vebitset_deserialize(const char *buf, size_t len) {
    if (buf == nullptr) {
        return nullptr;
    }
    std::string_view view{buf, len};
    vebitset_t p = static_cast<vebitset_t>(malloc(sizeof(VebTree)));
    if (p == nullptr) {
        return nullptr;
    }
    try {
        std::construct_at(p, VebTree::deserialize(view));
        return p;
    } catch (const std::exception&) {
        free(p);
        return nullptr;
    }
}
