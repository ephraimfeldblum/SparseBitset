/**
 * @file VebTree.h
 * @brief C API for van Emde Boas Tree implementation
 *
 * This header provides a C-compatible interface to the C++ VebTree template class.
 */

#ifndef VEBTREE_H
#define VEBTREE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to a VebTree instance
 */
typedef struct VebTree_Handle* VebTree_Handle_t;

/**
 * @brief Result structure for operations that return an optional size_t
 */
typedef struct {
    bool has_value;
    size_t value;
} VebTree_OptionalSize_t;

/**
 * @brief Memory statistics structure
 */
typedef struct {
    size_t total_clusters;
    size_t max_depth;
    size_t total_nodes;
} VebTree_MemoryStats_t;

/**
 * @brief Function table for VebTree operations
 *
 * This structure contains function pointers for all VebTree operations.
 * It allows users to get an API object for a specific VEB instance and then
 * use that API with the instance without needing to know the specific implementation.
 *
 * Example usage:
 * ```c
 * // Create a VEB instance
 * VebTree_Handle_t veb = vebtree_create(VEBTREE_STD, 100);
 *
 * // Get the API for this instance
 * const VebTree_API_t* api = vebtree_get_api(veb);
 *
 * // Use the API without knowing the implementation
 * api->insert(veb, 42);
 * bool exists = api->contains(veb, 42);
 * VEBTREE_DESTROY(veb);
 * ```
 */
typedef struct VebTree_API {
    /* Insert an element */
    void (*insert)(VebTree_Handle_t handle, size_t x);

    /* Remove an element */
    void (*remove)(VebTree_Handle_t handle, size_t x);

    /* Check if an element exists */
    bool (*contains)(VebTree_Handle_t handle, size_t x);

    /* Find the smallest element greater than x */
    VebTree_OptionalSize_t (*successor)(VebTree_Handle_t handle, size_t x);

    /* Find the largest element smaller than x */
    VebTree_OptionalSize_t (*predecessor)(VebTree_Handle_t handle, size_t x);

    /* Get the minimum element */
    VebTree_OptionalSize_t (*min)(VebTree_Handle_t handle);

    /* Get the maximum element */
    VebTree_OptionalSize_t (*max)(VebTree_Handle_t handle);

    /* Check if the tree is empty */
    bool (*empty)(VebTree_Handle_t handle);

    /* Clear all elements */
    void (*clear)(VebTree_Handle_t handle);

    /* Get the number of elements */
    size_t (*size)(VebTree_Handle_t handle);

    /* Convert to array */
    size_t* (*to_array)(VebTree_Handle_t handle);

    /* Get memory usage statistics */
    VebTree_MemoryStats_t (*get_memory_stats)(VebTree_Handle_t handle);

    /* Get the amount of currently allocated bytes */
    size_t (*get_allocated_memory)(VebTree_Handle_t handle);

    /* Get the current universe size */
    size_t (*universe_size)(VebTree_Handle_t handle);

    /* Set operations */
    bool (*equals)(VebTree_Handle_t handle1, VebTree_Handle_t handle2);

    /* Create intersection of two trees (elements in both) */
    void (*intersection)(VebTree_Handle_t handle1, VebTree_Handle_t handle2);

    /* Create union of two trees (elements in either) */
    void (*union_op)(VebTree_Handle_t handle1, VebTree_Handle_t handle2);

    /* Create symmetric difference of two trees (elements in exactly one) */
    void (*symmetric_difference)(VebTree_Handle_t handle1, VebTree_Handle_t handle2);

    /* Destroy the handle */
    void (*destroy)(VebTree_Handle_t handle);
} VebTree_API_t;

/**
 * @brief Factory function to create a VebTree instance with specified implementation
 *
 * @param impl_type The implementation type to use. Must be one of the VEBTREE_* constants.
 * @return VebTree_Handle_t Handle to the created instance. If creation fails, returns NULL.
 *
 * Note: Every call to vebtree_create() must be matched with a call to VEBTREE_DESTROY().
 */
VebTree_Handle_t vebtree_create();

/**
 * @brief Get the API function table for a specific VebTree instance
 *
 * This function returns a pointer to a function table that contains all the operations
 * for the specific implementation of the given handle. The user can then use this API
 * without needing to know which implementation is being used.
 *
 * This function always succeeds for valid handles returned by vebtree_create().
 * Calling it with a NULL handle results in undefined behavior.
 *
 * Example usage:
 * ```c
 * VebTree_Handle_t veb = vebtree_create(VEBTREE_STD);
 * if (veb) {
 *     const VebTree_API_t *api = vebtree_get_api(veb);
 *     bool exists = api->contains(veb, 42);
 * }
 * VEBTREE_DESTROY(veb);
 * ```
 *
 * @param handle The VebTree handle
 * @return const VebTree_API_t* Pointer to the API function table.
 */
const VebTree_API_t* vebtree_get_api(VebTree_Handle_t handle);

/**
 * @brief Safe destruction macro for VebTree handles
 *
 * This macro safely destroys a VebTree handle and sets the pointer to NULL,
 * preventing accidental reuse of freed handles. This follows the common C idioms
 * for safe resource cleanup.
 *
 * Example usage:
 * ```c
 * VebTree_Handle_t veb = vebtree_create(VEBTREE_STD, 100);
 * if (veb) {
 *     // Use the VEB...
 *     vebtree_get_api(veb)->insert(veb, 42);
 * }
 * // Safe cleanup - veb will be NULL after this
 * VEBTREE_DESTROY(veb);
 *
 * // veb is now NULL, safe to check or reuse
 * assert(veb == NULL);
 * ```
 *
 * @param handle_ptr Pointer to the VebTree handle (will be set to NULL after destruction)
 */
#define VEBTREE_DESTROY(handle_ptr)                           \
    do {                                                      \
        if ((handle_ptr) != NULL) {                           \
            vebtree_get_api(handle_ptr)->destroy(handle_ptr); \
            (handle_ptr) = NULL;                              \
        }                                                     \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif /* VEBTREE_H */
