/**
 * @file vebitset.h
 * @brief C API for vEBitset - van Emde Boas Tree implementation
 *
 * Public C interface for the vEBitset library (libvebitset).
 * Provides efficient bitset operations with O(log log U) time complexity.
 */

#ifndef VEBITSET_H
#define VEBITSET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a VEB tree bitset instance
 */
typedef struct VebTree* vebitset_t;
typedef const struct VebTree* const_vebitset_t;

/**
 * @brief Result structure for operations that return an optional size_t
 */
typedef struct {
    bool has_value;
    size_t value;
} vebitset_optional_t;

/**
 * @brief Memory statistics structure
 */
typedef struct {
    size_t total_clusters;
    size_t max_depth;
    size_t total_nodes;
} vebitset_stats_t;

/**
 * @brief Create a new empty bitset
 *
 * @return vebitset_t Handle to the created bitset, or NULL on failure.
 *
 * Note: Every call to vebitset_create() must be matched with a call to vebitset_destroy().
 */
vebitset_t vebitset_create(void);

/**
 * @brief Destroy a bitset handle
 *
 * @param handle The bitset to destroy
 */
void vebitset_destroy(vebitset_t handle);

/**
 * @brief Insert an element into the bitset
 *
 * @param handle The bitset
 * @param x The element to insert
 */
void vebitset_insert(vebitset_t handle, size_t x);

/**
 * @brief Remove an element from the bitset
 *
 * @param handle The bitset
 * @param x The element to remove
 */
void vebitset_remove(vebitset_t handle, size_t x);

/**
 * @brief Check if an element exists in the bitset
 *
 * @param handle The bitset
 * @param x The element to search for
 * @return true if the element exists, false otherwise
 */
bool vebitset_contains(const_vebitset_t handle, size_t x);

/**
 * @brief Find the smallest element greater than x
 *
 * @param handle The bitset
 * @param x The reference element
 * @return Optional containing the successor if it exists
 */
vebitset_optional_t vebitset_successor(const_vebitset_t handle, size_t x);

/**
 * @brief Find the largest element smaller than x
 *
 * @param handle The bitset
 * @param x The reference element
 * @return Optional containing the predecessor if it exists
 */
vebitset_optional_t vebitset_predecessor(const_vebitset_t handle, size_t x);

/**
 * @brief Get the minimum element in the bitset
 *
 * @param handle The bitset
 * @return Optional containing the minimum element if the bitset is not empty
 */
vebitset_optional_t vebitset_min(const_vebitset_t handle);

/**
 * @brief Get the maximum element in the bitset
 *
 * @param handle The bitset
 * @return Optional containing the maximum element if the bitset is not empty
 */
vebitset_optional_t vebitset_max(const_vebitset_t handle);

/**
 * @brief Check if the bitset is empty
 *
 * @param handle The bitset
 * @return true if empty, false otherwise
 */
bool vebitset_is_empty(const_vebitset_t handle);

/**
 * @brief Clear all elements from the bitset
 *
 * @param handle The bitset
 */
void vebitset_clear(vebitset_t handle);

/**
 * @brief Get the number of elements in the bitset
 *
 * @param handle The bitset
 * @return The count of elements
 */
size_t vebitset_count(const_vebitset_t handle);

/**
 * @brief Count elements in the inclusive range [start, end]
 *
 * @param handle The bitset
 * @param start The start of the range (inclusive)
 * @param end The end of the range (inclusive)
 * @return The count of elements in the range
 */
size_t vebitset_count_range(const_vebitset_t handle, size_t start, size_t end);

/**
 * @brief Convert bitset to array (caller must free)
 *
 * @param handle The bitset
 * @param out_len Output parameter for array length
 * @return Pointer to allocated array, or NULL if empty or on error. Caller must free.
 */
size_t* vebitset_to_array(const_vebitset_t handle, size_t *out_len);

/**
 * @brief Get memory usage statistics
 *
 * @param handle The bitset
 * @return Statistics structure
 */
vebitset_stats_t vebitset_get_stats(const_vebitset_t handle);

/**
 * @brief Get the amount of currently allocated bytes
 *
 * @param handle The bitset
 * @return Number of allocated bytes
 */
size_t vebitset_allocated_memory(const_vebitset_t handle);

/**
 * @brief Get the current universe size
 *
 * @param handle The bitset
 * @return The universe size (maximum value that can be stored + 1)
 */
size_t vebitset_universe_size(const_vebitset_t handle);

/**
 * @brief Check if two bitsets are equal
 *
 * @param handle1 First bitset
 * @param handle2 Second bitset
 * @return true if both bitsets contain the same elements, false otherwise
 */
bool vebitset_equals(const_vebitset_t handle1, const_vebitset_t handle2);

/**
 * @brief Replace handle1 with the intersection of handle1 and handle2
 *
 * Sets handle1 to contain only elements that exist in both bitsets.
 *
 * @param handle1 The destination bitset (modified in-place)
 * @param handle2 The source bitset
 */
void vebitset_intersection(vebitset_t handle1, const_vebitset_t handle2);

/**
 * @brief Replace handle1 with the union of handle1 and handle2
 *
 * Sets handle1 to contain all elements that exist in either bitset.
 *
 * @param handle1 The destination bitset (modified in-place)
 * @param handle2 The source bitset
 */
void vebitset_union(vebitset_t handle1, const_vebitset_t handle2);

/**
 * @brief Replace handle1 with the symmetric difference of handle1 and handle2
 *
 * Sets handle1 to contain only elements that exist in exactly one of the bitsets.
 *
 * @param handle1 The destination bitset (modified in-place)
 * @param handle2 The source bitset
 */
void vebitset_symmetric_difference(vebitset_t handle1, const_vebitset_t handle2);

/**
 * @brief Move contents from src to dst
 *
 * Transfers ownership: dst becomes a copy of src, and src is left in a moved-from state.
 *
 * @param dst The destination bitset
 * @param src The source bitset
 */
void vebitset_move(vebitset_t dst, vebitset_t src);

/**
 * @brief Serialize a bitset to a buffer
 *
 * @param handle The bitset handle
 * @param out_len Output parameter for the serialized data length
 * @return Pointer to allocated buffer (caller must free), or NULL on error
 */
const char* vebitset_serialize(vebitset_t handle, size_t *out_len);

/**
 * @brief Deserialize a bitset from a buffer
 *
 * @param buf Buffer containing serialized data
 * @param len Length of the buffer
 * @return Handle to the deserialized bitset, or NULL on error
 */
vebitset_t vebitset_deserialize(const char *buf, size_t len);



#ifdef __cplusplus
}
#endif

#endif /* VEBITSET_H */
