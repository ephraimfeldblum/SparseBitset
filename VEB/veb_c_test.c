/**
 * @file veb_c_test.c
 * @brief Test suite for the VebTree C API
 *
 * This file contains comprehensive tests for the VebTree C API,
 * including data structure operations, memory usage, and performance.
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <limits.h>
#include <stdbool.h>
#include <assert.h>
#include "VebTree.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

// < 2^8
const size_t primes[] = {
    2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37, 41, 43, 47, 53, 59, 61, 67, 71, 73, 79, 83, 89, 97, 101, 103, 107, 109,
    113, 127, 131, 137, 139, 149, 151, 157, 163, 167, 173, 179, 181, 191, 193, 197, 199, 211, 223, 227, 229, 233, 239,
    241, 251,
};
// < 2^16
const size_t fibonacci[] = {
    0, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89, 144, 233, 377, 610, 987, 1597, 2584, 4181, 6765, 10946, 17711, 28657, 46368,
};
// < 2^64
const size_t powers_of_2[] = {
    0x1, 0x2, 0x4, 0x8, 0x10, 0x20, 0x40, 0x80,
    0x100, 0x200, 0x400, 0x800, 0x1000, 0x2000, 0x4000, 0x8000,
    0x10000, 0x20000, 0x40000, 0x80000, 0x100000, 0x200000, 0x400000, 0x800000,
    0x1000000, 0x2000000, 0x4000000, 0x8000000, 0x10000000, 0x20000000, 0x40000000, 0x80000000,
    0x100000000, 0x200000000, 0x400000000, 0x800000000, 0x1000000000, 0x2000000000, 0x4000000000, 0x8000000000,
    0x10000000000, 0x20000000000, 0x40000000000, 0x80000000000, 0x100000000000, 0x200000000000, 0x400000000000, 0x800000000000,
    0x1000000000000, 0x2000000000000, 0x4000000000000, 0x8000000000000, 0x10000000000000, 0x20000000000000, 0x40000000000000, 0x80000000000000,
    0x100000000000000, 0x200000000000000, 0x400000000000000, 0x800000000000000, 0x1000000000000000, 0x2000000000000000, 0x4000000000000000, 0x8000000000000000,
};

/**
 * @brief Generic function that works with any VebTree implementation
 *
 * This function demonstrates the key benefit: you can write code that works
 * with any implementation without knowing which one is being used.
 * Only the handle is needed - the API is obtained when required.
 */
void process_data_set(VebTree_Handle_t veb, const size_t* data, size_t data_size, const char* description) {
    const VebTree_API_t* api = vebtree_get_api(veb);

    printf("\n=== Processing %s ===\n", description);
    printf("Implementation: %s\n", api->hash_table_name());
    
    for (size_t i = data_size; i > 0; --i) {
        api->insert(veb, data[i - 1]);
    }
    assert(api->size(veb) == data_size);
    for (size_t i = 0; i < data_size; i++) {
        assert(api->contains(veb, data[i]));
    }
    
    VebTree_OptionalSize_t min_val = api->min(veb);
    VebTree_OptionalSize_t max_val = api->max(veb);
    assert(min_val.has_value);
    assert(max_val.has_value);
    assert(min_val.value == data[0]);
    assert(max_val.value == data[data_size - 1]);

    printf("Testing successor/predecessor operations...\n");

    VebTree_OptionalSize_t no_succ = api->successor(veb, max_val.value);
    assert(!no_succ.has_value);

    VebTree_OptionalSize_t no_pred = api->predecessor(veb, min_val.value);
    assert(!no_pred.has_value);

    VebTree_OptionalSize_t min_succ = api->successor(veb, min_val.value);
    if (data_size > 1) {
        assert(min_succ.has_value);
        assert(min_succ.value > min_val.value);
        assert(api->contains(veb, min_succ.value));
    }

    VebTree_OptionalSize_t max_pred = api->predecessor(veb, max_val.value);
    if (data_size > 1) {
        assert(max_pred.has_value);
        assert(max_pred.value < max_val.value);
        assert(api->contains(veb, max_pred.value));
    }

    for (size_t i = 0; i < data_size; i++) {
        VebTree_OptionalSize_t succ = api->successor(veb, data[i]);
        VebTree_OptionalSize_t pred = api->predecessor(veb, data[i]);

        if (i < data_size - 1) {
            assert(succ.has_value);
            assert(succ.value == data[i + 1]);
        } else {
            assert(!succ.has_value);
        }

        if (i > 0) {
            assert(pred.has_value);
            assert(pred.value == data[i - 1]);
        } else {
            assert(!pred.has_value);
        }
    }

    size_t test_points[] = {0, 1, 10, 50, 100, 500, 1000, 10000, 100000, 1000000};
    for (size_t i = 0; i < ARRAY_SIZE(test_points); i++) {
        size_t test_val = test_points[i];

        VebTree_OptionalSize_t succ = api->successor(veb, test_val);
        VebTree_OptionalSize_t pred = api->predecessor(veb, test_val);

        if (succ.has_value) {
            assert(succ.value > test_val);
            assert(api->contains(veb, succ.value));

            VebTree_OptionalSize_t succ_pred = api->predecessor(veb, succ.value);
            if (api->contains(veb, test_val)) {
                assert(succ_pred.has_value && succ_pred.value == test_val);
            } else {
                assert(!succ_pred.has_value || succ_pred.value < test_val);
            }
        }

        if (pred.has_value) {
            assert(pred.value < test_val);
            assert(api->contains(veb, pred.value));

            VebTree_OptionalSize_t pred_succ = api->successor(veb, pred.value);
            if (api->contains(veb, test_val)) {
                assert(pred_succ.has_value && pred_succ.value == test_val);
            } else {
                assert(!pred_succ.has_value || pred_succ.value > test_val);
            }
        }
    }

    size_t middle = min_val.value + (max_val.value - min_val.value) / 2;
    VebTree_OptionalSize_t middle_succ = api->successor(veb, middle);
    assert(middle_succ.has_value);

    size_t high_val = max_val.value - (max_val.value - min_val.value) / 4;
    VebTree_OptionalSize_t high_pred = api->predecessor(veb, high_val);
    assert(high_pred.has_value);

    size_t test_values[] = {
        min_val.value, max_val.value, 111111, 333333, 999999, middle_succ.value, high_pred.value,
    };
    bool expected_results[] = {
        true, true, false, false, false, true, true,
    };
    for (size_t i = 0; i < ARRAY_SIZE(test_values); i++) {
        assert(api->contains(veb, test_values[i]) == expected_results[i]);
    }

    VebTree_MemoryStats_t stats = api->get_memory_stats(veb);
    printf("Memory usage: %zu nodes, %zu clusters, max depth %zu\n",
           stats.total_nodes, stats.total_clusters, stats.max_depth);
    size_t allocated_memory = api->get_allocated_memory(veb) * CHAR_BIT;
    printf("Allocated memory: %zu bits\n", allocated_memory);
    printf("                = %.2f / element\n", (double)allocated_memory / (double)api->size(veb));
    printf("                = %.2e / universe size\n\n", (double)allocated_memory / (double)api->universe_size(veb));

    size_t original_size = api->size(veb);
    size_t removed_count = 0;
    for (size_t i = 2; i < data_size; i += 3) {
        api->remove(veb, data[i]);
        removed_count++;
    }
    size_t new_size = api->size(veb);
    assert(new_size == original_size - removed_count);

    for (size_t i = 0; i < data_size; i++) {
        if (i % 3 != 2) {
            assert(api->contains(veb, data[i]));
        } else {
            assert(!api->contains(veb, data[i]));
        }
    }

    if (new_size > 0) {
        printf("Testing successor/predecessor after removals...\n");
        VebTree_OptionalSize_t new_min = api->min(veb);
        VebTree_OptionalSize_t new_max = api->max(veb);
        assert(new_min.has_value && new_max.has_value);

        VebTree_OptionalSize_t no_succ_after = api->successor(veb, new_max.value);
        assert(!no_succ_after.has_value);

        VebTree_OptionalSize_t no_pred_after = api->predecessor(veb, new_min.value);
        assert(!no_pred_after.has_value);

        size_t remaining_elements[data_size];
        size_t remaining_count = 0;
        for (size_t i = 0; i < data_size; i++) {
            if (i % 3 != 2) {
                remaining_elements[remaining_count++] = data[i];
            }
        }

        for (size_t i = 0; i < remaining_count; i++) {
            VebTree_OptionalSize_t succ = api->successor(veb, remaining_elements[i]);
            VebTree_OptionalSize_t pred = api->predecessor(veb, remaining_elements[i]);

            if (i < remaining_count - 1) {
                assert(succ.has_value);
                assert(succ.value == remaining_elements[i + 1]);
            } else {
                assert(!succ.has_value);
            }

            if (i > 0) {
                assert(pred.has_value);
                assert(pred.value == remaining_elements[i - 1]);
            } else {
                assert(!pred.has_value);
            }
        }

        for (size_t i = 0; i < data_size; i++) {
            if (i % 3 == 2) {
                VebTree_OptionalSize_t removed_succ = api->successor(veb, data[i]);
                VebTree_OptionalSize_t removed_pred = api->predecessor(veb, data[i]);

                if (removed_succ.has_value) {
                    assert(api->contains(veb, removed_succ.value));
                    assert(removed_succ.value > data[i]);
                }

                if (removed_pred.has_value) {
                    assert(api->contains(veb, removed_pred.value));
                    assert(removed_pred.value < data[i]);
                }
            }
        }

        size_t test_middle = (new_min.value + new_max.value) / 2;
        VebTree_OptionalSize_t post_removal_succ = api->successor(veb, test_middle);
        assert(post_removal_succ.has_value);
    }

    VebTree_MemoryStats_t post_removal_stats = api->get_memory_stats(veb);
    size_t post_removal_memory = api->get_allocated_memory(veb) * CHAR_BIT;

    printf("Post-removal memory usage: %zu nodes, %zu clusters, max depth %zu\n",
           post_removal_stats.total_nodes, post_removal_stats.total_clusters, post_removal_stats.max_depth);
    printf("Post-removal allocated memory: %zu bits\n", post_removal_memory);
    if (new_size > 0) {
        printf("                            = %.2f / element\n", (double)post_removal_memory / (double)new_size);
    }

    long long memory_change = (long long)post_removal_memory - (long long)allocated_memory;
    double memory_change_percent = ((double)post_removal_memory - (double)allocated_memory) / (double)allocated_memory * 100.0;
    printf("Memory change: %lld bits (%+.1f%%)\n", memory_change, memory_change_percent);

    long long node_change = (long long)post_removal_stats.total_nodes - (long long)stats.total_nodes;
    long long cluster_change = (long long)post_removal_stats.total_clusters - (long long)stats.total_clusters;
    printf("Node change: %lld nodes, Cluster change: %lld clusters\n\n", node_change, cluster_change);

    api->clear(veb);
}

void process_data_sets() {
    VebTree_ImplType_t implementations[] = {
        VEBTREE_STD,
        VEBTREE_ABSL,
        VEBTREE_BOOST_FLAT,
        VEBTREE_BOOST_NODE,
        VEBTREE_BOOST,
    };

    for (size_t impl = 0; impl < ARRAY_SIZE(implementations); impl++) {
        VebTree_Handle_t veb = vebtree_create(implementations[impl]);
        if (!veb) {
            printf("\nFailed to create VEB tree with implementation %d\n\n", implementations[impl]);
            continue;
        }

        process_data_set(veb, primes, ARRAY_SIZE(primes), "Prime numbers");
        fflush(stdout);
        process_data_set(veb, fibonacci, ARRAY_SIZE(fibonacci), "Fibonacci numbers");
        fflush(stdout);
        process_data_set(veb, powers_of_2, ARRAY_SIZE(powers_of_2), "Powers of 2");
        fflush(stdout);

        VEBTREE_DESTROY(veb);
    }
}

void benchmark_implementation(VebTree_Handle_t veb) {
    const VebTree_API_t* api = vebtree_get_api(veb);
    printf("\n=== Benchmarking %s ===\n", api->hash_table_name());
    
    const size_t num_operations = 1000000;
    clock_t start, end;
    
    // Benchmark insertions
    start = clock();
    for (size_t i = 0; i < num_operations; i++) {
        api->insert(veb, i * 7 % ((1ULL << 32) - 1));  // Some pseudo-random pattern
    }
    end = clock();
    
    double insert_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Inserted %zu elements in %.4f seconds\n", num_operations, insert_time);
    assert(api->size(veb) == num_operations);

    // Benchmark lookups
    start = clock();
    size_t found_count = 0;
    for (size_t i = 0; i < num_operations; i++) {
        if (api->contains(veb, i * 11 % ((1ULL << 32) - 1))) {
            found_count++;
        }
    }
    end = clock();
    
    double lookup_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Performed %zu lookups in %.4f seconds (found %zu)\n", 
           num_operations, lookup_time, found_count);
    
    // Benchmark successor operations
    start = clock();
    size_t successor_count = 0;
    for (size_t i = 0; i < num_operations / 10; i++) {
        VebTree_OptionalSize_t succ = api->successor(veb, i * 13 % ((1ULL << 32) - 1));
        if (succ.has_value) {
            successor_count++;
        }
    }
    end = clock();

    double successor_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Performed %zu successor operations in %.4f seconds (found %zu)\n",
           num_operations / 10, successor_time, successor_count);

    size_t allocated_memory = api->get_allocated_memory(veb) * CHAR_BIT;
    printf("Allocated memory: %zu bits\n", allocated_memory);
    printf("                = %.2f / element\n", (double)allocated_memory / (double)api->size(veb));
    printf("                = %.2e / universe size\n\n", (double)allocated_memory / (double)api->universe_size(veb));

    api->clear(veb);
}

void benchmark_implementations() {
    VebTree_ImplType_t implementations[] = {
        VEBTREE_STD,
        VEBTREE_ABSL,
        VEBTREE_BOOST_FLAT,
        VEBTREE_BOOST_NODE,
        VEBTREE_BOOST,
    };

    for (size_t impl = 0; impl < ARRAY_SIZE(implementations); impl++) {
        VebTree_Handle_t veb = vebtree_create(implementations[impl]);
        if (!veb) {
            printf("\nFailed to create VEB tree with implementation %d\n\n", implementations[impl]);
            continue;
        }

        benchmark_implementation(veb);
        fflush(stdout);
        VEBTREE_DESTROY(veb);
    }
}

void test_set_operations() {
    VebTree_Handle_t tree1 = vebtree_create(VEBTREE_STD);
    VebTree_Handle_t tree2 = vebtree_create(VEBTREE_STD);
    assert(tree1 && tree2);

    const VebTree_API_t* api = vebtree_get_api(tree1);

    for (size_t i = 0; i < ARRAY_SIZE(fibonacci); i++) {
        api->insert(tree1, fibonacci[i]);
    }

    for (size_t i = 0; i < ARRAY_SIZE(powers_of_2); i++) {
        api->insert(tree2, powers_of_2[i]);
    }

    api->intersection(tree1, tree2);
    size_t intersection_size = api->size(tree1);

    assert(intersection_size == 3);
    VEBTREE_DESTROY(tree1);

    tree1 = vebtree_create(VEBTREE_STD);
    for (size_t i = 0; i < ARRAY_SIZE(fibonacci); i++) {
        api->insert(tree1, fibonacci[i]);
    }

    api->union_op(tree1, tree2);
    size_t union_size = api->size(tree1);

    assert(union_size == ARRAY_SIZE(fibonacci) + ARRAY_SIZE(powers_of_2) - 3);    
    VEBTREE_DESTROY(tree1);

    tree1 = vebtree_create(VEBTREE_STD);
    for (size_t i = 0; i < ARRAY_SIZE(fibonacci); i++) {
        api->insert(tree1, fibonacci[i]);
    }

    api->symmetric_difference(tree1, tree2);
    size_t xor_size = api->size(tree1);

    assert(xor_size == ARRAY_SIZE(fibonacci) + ARRAY_SIZE(powers_of_2) - 6);
    VEBTREE_DESTROY(tree1);
    VEBTREE_DESTROY(tree2);
}

void test_growth_optimization() {
    VebTree_Handle_t tree = vebtree_create(VEBTREE_STD);
    const VebTree_API_t* api = vebtree_get_api(tree);

    // Test 1: Node8 -> Node16 with cluster movement
    size_t elements[] = {0, 1, 5, 10, 50, 100, 150, 200, 255};
    for (size_t i = 0; i < ARRAY_SIZE(elements); i++) {
        api->insert(tree, elements[i]);
    }
    api->insert(tree, 256);  // Trigger growth

    assert(api->size(tree) == ARRAY_SIZE(elements) + 1);
    for (size_t i = 0; i < ARRAY_SIZE(elements); i++) {
        assert(api->contains(tree, elements[i]));
    }
    assert(api->contains(tree, 256));
    VEBTREE_DESTROY(tree);

    // Test 2: Edge case with only min/max
    tree = vebtree_create(VEBTREE_STD);
    api->insert(tree, 0);
    api->insert(tree, 255);
    api->insert(tree, 256);  // Trigger growth

    assert(api->size(tree) == 3);
    assert(api->contains(tree, 0));
    assert(api->contains(tree, 255));
    assert(api->contains(tree, 256));
    VEBTREE_DESTROY(tree);

    // Test 3: Single element growth
    tree = vebtree_create(VEBTREE_STD);
    api->insert(tree, 100);
    api->insert(tree, 300);  // Trigger growth

    assert(api->size(tree) == 2);
    assert(api->contains(tree, 100));
    assert(api->contains(tree, 300));
    VEBTREE_DESTROY(tree);

    // Test 4: Multi-level growth
    tree = vebtree_create(VEBTREE_STD);
    api->insert(tree, 10);
    api->insert(tree, 200);
    api->insert(tree, 100000);  // Trigger multiple levels of growth

    assert(api->size(tree) == 3);
    assert(api->contains(tree, 10));
    assert(api->contains(tree, 200));
    assert(api->contains(tree, 100000));

    // Test successor/predecessor operations after growth
    VebTree_OptionalSize_t succ = api->successor(tree, 10);
    assert(succ.has_value && succ.value == 200);
    succ = api->successor(tree, 200);
    assert(succ.has_value && succ.value == 100000);

    VebTree_OptionalSize_t pred = api->predecessor(tree, 100000);
    assert(pred.has_value && pred.value == 200);
    pred = api->predecessor(tree, 200);
    assert(pred.has_value && pred.value == 10);

    VebTree_OptionalSize_t min_val = api->min(tree);
    VebTree_OptionalSize_t max_val = api->max(tree);
    assert(min_val.has_value && min_val.value == 10);
    assert(max_val.has_value && max_val.value == 100000);

    VEBTREE_DESTROY(tree);
}

int main() {
    process_data_sets();
    fflush(stdout);
    benchmark_implementations();
    fflush(stdout);
    test_set_operations();
    fflush(stdout);
    test_growth_optimization();
    fflush(stdout);

    printf("\nAll tests completed successfully!\n\n");
    return 0;
}
