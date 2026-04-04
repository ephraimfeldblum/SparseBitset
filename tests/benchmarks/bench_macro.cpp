#include "benchmark.hpp"
#include "VEB/VebTree.hpp"
#include <nanobench.h>
#include <roaring/roaring.hh>
#include <vector>
#include <algorithm>
#include <random>
#include <fstream>

extern std::ofstream g_output;

std::vector<uint32_t> generate_clustered_values(uint32_t count, uint32_t universe_size) {
    std::vector<uint32_t> values;
    values.reserve(count);
    std::mt19937_64 gen(42);
    std::uniform_int_distribution<uint32_t> cluster_dist(0, (universe_size / 256) ? universe_size / 256 - 1 : 0);
    
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t cluster = cluster_dist(gen);
        uint32_t offset = gen() % 256;
        uint32_t val = cluster * 256 + offset;
        if (val < universe_size) {
            values.push_back(static_cast<uint32_t>(val));
        }
    }
    return values;
}

std::vector<uint32_t> generate_sequential_values(uint32_t count, uint32_t universe_size) {
    std::vector<uint32_t> values;
    values.reserve(count);
    
    uint32_t step = universe_size / count;
    if (step == 0) step = 1;
    
    for (uint32_t i = 0; i < count; ++i) {
        values.push_back((i * step) % universe_size);
    }
    return values;
}

void bench_macro_insert() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t ELEMENT_COUNT = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        
        auto test_data = generate_random_values(ELEMENT_COUNT, UNIVERSE_SIZE);

        VebTree veb_tree;
        std::vector<bool> vec_bool(UNIVERSE_SIZE);
        roaring::Roaring roaring_tree;
        
        auto veb_bench = run_macro_benchmark(
            "veb_insert_random_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "insert_random",
            density,
            ELEMENT_COUNT,
            [&] {
                veb_tree.clear();
                for (uint32_t val : test_data) {
                    veb_tree.insert(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&veb_tree);
            },
            [&] { return veb_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_bench, g_output);

        auto vec_bench = run_macro_benchmark(
            "vector_bool_insert_random_" + std::to_string(static_cast<int>(density * 100)),
            "vector<bool>",
            "insert_random",
            density,
            ELEMENT_COUNT,
            [&] {
                std::fill(vec_bool.begin(), vec_bool.end(), false);
                for (uint32_t val : test_data) {
                    vec_bool[val] = true;
                }
                ankerl::nanobench::doNotOptimizeAway(&vec_bool);
            },
            [&] { return vec_bool.capacity() / 8; });
        ankerl::nanobench::render(benchmark_json_template(), vec_bench, g_output);

        auto roaring_bench = run_macro_benchmark(
            "roaring_insert_random_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "insert_random",
            density,
            ELEMENT_COUNT,
            [&] {
                roaring_tree.clear();
                for (uint32_t val : test_data) {
                    roaring_tree.add(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&roaring_tree);
            },
            [&] { return roaring_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_bench, g_output);
    }
}

void bench_macro_insert_sequential() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t ELEMENT_COUNT = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        
        auto test_data = generate_sequential_values(ELEMENT_COUNT, UNIVERSE_SIZE);

        VebTree veb_tree;
        std::vector<bool> vec_bool(UNIVERSE_SIZE);
        roaring::Roaring roaring_tree;
        
        auto veb_bench = run_macro_benchmark(
            "veb_insert_seq_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "insert_sequential",
            density,
            ELEMENT_COUNT,
            [&] {
                veb_tree.clear();
                for (uint32_t val : test_data) {
                    veb_tree.insert(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&veb_tree);
            },
            [&] { return veb_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_bench, g_output);

        auto vec_bench = run_macro_benchmark(
            "vector_bool_insert_seq_" + std::to_string(static_cast<int>(density * 100)),
            "vector<bool>",
            "insert_sequential",
            density,
            ELEMENT_COUNT,
            [&] {
                std::fill(vec_bool.begin(), vec_bool.end(), false);
                for (uint32_t val : test_data) {
                    vec_bool[val] = true;
                }
                ankerl::nanobench::doNotOptimizeAway(&vec_bool);
            },
            [&] { return vec_bool.capacity() / 8; });
        ankerl::nanobench::render(benchmark_json_template(), vec_bench, g_output);

        auto roaring_bench = run_macro_benchmark(
            "roaring_insert_seq_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "insert_sequential",
            density,
            ELEMENT_COUNT,
            [&] {
                roaring_tree.clear();
                for (uint32_t val : test_data) {
                    roaring_tree.add(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&roaring_tree);
            },
            [&] { return roaring_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_bench, g_output);
    }
}

void bench_macro_insert_clustered() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t ELEMENT_COUNT = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        
        auto test_data = generate_clustered_values(ELEMENT_COUNT, UNIVERSE_SIZE);

        VebTree veb_tree;
        std::vector<bool> vec_bool(UNIVERSE_SIZE);
        roaring::Roaring roaring_tree;
        
        auto veb_bench = run_macro_benchmark(
            "veb_insert_clustered_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "insert_clustered",
            density,
            ELEMENT_COUNT,
            [&] {
                veb_tree.clear();
                for (uint32_t val : test_data) {
                    veb_tree.insert(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&veb_tree);
            },
            [&] { return veb_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_bench, g_output);

        auto vec_bench = run_macro_benchmark(
            "vector_bool_insert_clustered_" + std::to_string(static_cast<int>(density * 100)),
            "vector<bool>",
            "insert_clustered",
            density,
            ELEMENT_COUNT,
            [&] {
                std::fill(vec_bool.begin(), vec_bool.end(), false);
                for (uint32_t val : test_data) {
                    vec_bool[val] = true;
                }
                ankerl::nanobench::doNotOptimizeAway(&vec_bool);
            },
            [&] { return vec_bool.capacity() / 8; });
        ankerl::nanobench::render(benchmark_json_template(), vec_bench, g_output);

        auto roaring_bench = run_macro_benchmark(
            "roaring_insert_clustered_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "insert_clustered",
            density,
            ELEMENT_COUNT,
            [&] {
                roaring_tree.clear();
                for (uint32_t val : test_data) {
                    roaring_tree.add(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&roaring_tree);
            },
            [&] { return roaring_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_bench, g_output);
    }
}

void bench_macro_contains() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t ELEMENT_COUNT = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        const uint32_t QUERY_COUNT = 100000;
        
        auto insert_data = generate_random_values(ELEMENT_COUNT, UNIVERSE_SIZE);
        auto query_data = generate_random_values(QUERY_COUNT, UNIVERSE_SIZE);

        VebTree veb_tree;
        for (uint32_t val : insert_data) {
            veb_tree.insert(val);
        }
        
        std::vector<bool> vec_bool(UNIVERSE_SIZE);
        for (uint32_t val : insert_data) {
            vec_bool[val] = true;
        }
        
        roaring::Roaring roaring_tree;
        for (uint32_t val : insert_data) {
            roaring_tree.add(val);
        }

        auto veb_bench = run_macro_benchmark(
            "veb_contains_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "contains",
            density,
            QUERY_COUNT,
            [&] {
                uint32_t count = 0;
                for (uint32_t val : query_data) {
                    count += veb_tree.contains(val) ? 1 : 0;
                }
                ankerl::nanobench::doNotOptimizeAway(count);
            },
            [&] { return veb_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_bench, g_output);

        auto vec_bench = run_macro_benchmark(
            "vector_bool_contains_" + std::to_string(static_cast<int>(density * 100)),
            "vector<bool>",
            "contains",
            density,
            QUERY_COUNT,
            [&] {
                uint32_t count = 0;
                for (uint32_t val : query_data) {
                    count += vec_bool[val] ? 1 : 0;
                }
                ankerl::nanobench::doNotOptimizeAway(count);
            },
            [&] { return vec_bool.capacity() / 8; });
        ankerl::nanobench::render(benchmark_json_template(), vec_bench, g_output);

        auto roaring_bench = run_macro_benchmark(
            "roaring_contains_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "contains",
            density,
            QUERY_COUNT,
            [&] {
                uint32_t count = 0;
                for (uint32_t val : query_data) {
                    count += roaring_tree.contains(val) ? 1 : 0;
                }
                ankerl::nanobench::doNotOptimizeAway(count);
            },
            [&] { return roaring_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_bench, g_output);
    }
}

void bench_macro_remove() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t ELEMENT_COUNT = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        const uint32_t REMOVE_COUNT = ELEMENT_COUNT / 2;
        
        auto insert_data = generate_random_values(ELEMENT_COUNT, UNIVERSE_SIZE);
        auto remove_data = generate_random_values(REMOVE_COUNT, UNIVERSE_SIZE);

        VebTree veb_tree;
        for (uint32_t val : insert_data) {
            veb_tree.insert(val);
        }
        
        std::vector<bool> vec_bool(UNIVERSE_SIZE);
        for (uint32_t val : insert_data) {
            vec_bool[val] = true;
        }
        
        roaring::Roaring roaring_tree;
        for (uint32_t val : insert_data) {
            roaring_tree.add(val);
        }

        auto veb_bench = run_macro_benchmark(
            "veb_remove_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "remove",
            density,
            REMOVE_COUNT,
            [&] {
                for (uint32_t val : remove_data) {
                    veb_tree.remove(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&veb_tree);
            },
            [&] { return veb_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_bench, g_output);

        auto vec_bench = run_macro_benchmark(
            "vector_bool_remove_" + std::to_string(static_cast<int>(density * 100)),
            "vector<bool>",
            "remove",
            density,
            REMOVE_COUNT,
            [&] {
                for (uint32_t val : remove_data) {
                    vec_bool[val] = false;
                }
                ankerl::nanobench::doNotOptimizeAway(&vec_bool);
            },
            [&] { return vec_bool.capacity() / 8; });
        ankerl::nanobench::render(benchmark_json_template(), vec_bench, g_output);

        auto roaring_bench = run_macro_benchmark(
            "roaring_remove_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "remove",
            density,
            REMOVE_COUNT,
            [&] {
                for (uint32_t val : remove_data) {
                    roaring_tree.remove(val);
                }
                ankerl::nanobench::doNotOptimizeAway(&roaring_tree);
            },
            [&] { return roaring_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_bench, g_output);
    }
}

void bench_macro_iteration() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t ELEMENT_COUNT = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        
        auto test_data = generate_random_values(ELEMENT_COUNT, UNIVERSE_SIZE);

        VebTree veb_tree;
        for (uint32_t val : test_data) {
            veb_tree.insert(val);
        }
        
        std::vector<bool> vec_bool(UNIVERSE_SIZE);
        for (uint32_t val : test_data) {
            vec_bool[val] = true;
        }
        
        roaring::Roaring roaring_tree;
        for (uint32_t val : test_data) {
            roaring_tree.add(val);
        }

        auto veb_bench = run_macro_benchmark(
            "veb_iteration_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "iteration",
            density,
            ELEMENT_COUNT,
            [&] {
                uint64_t sum = 0;
                for (uint64_t val : veb_tree) {
                    sum += val;
                }
                ankerl::nanobench::doNotOptimizeAway(sum);
            },
            [&] { return veb_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_bench, g_output);

        auto roaring_bench = run_macro_benchmark(
            "roaring_iteration_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "iteration",
            density,
            ELEMENT_COUNT,
            [&] {
                uint64_t sum = 0;
                for (uint32_t val : roaring_tree) {
                    sum += val;
                }
                ankerl::nanobench::doNotOptimizeAway(sum);
            },
            [&] { return roaring_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_bench, g_output);
    }
}

void bench_macro_set_operations() {
    std::vector<double> densities = {0.001, 0.01, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 0.99};
    
    for (double density : densities) {
        const uint32_t UNIVERSE_SIZE = 1000000;
        const uint32_t SET_SIZE = static_cast<uint32_t>(UNIVERSE_SIZE * density);
        
        auto data1 = generate_random_values(SET_SIZE, UNIVERSE_SIZE);
        auto data2 = generate_random_values(SET_SIZE, UNIVERSE_SIZE);

        VebTree veb_tree1, veb_tree2, veb_result_tree;
        for (uint32_t val : data1) {
            veb_tree1.insert(val);
        }
        for (uint32_t val : data2) {
            veb_tree2.insert(val);
        }
        
        roaring::Roaring roaring_tree1, roaring_tree2, roaring_result_tree;
        for (uint32_t val : data1) {
            roaring_tree1.add(val);
        }
        for (uint32_t val : data2) {
            roaring_tree2.add(val);
        }

        auto veb_union_bench = run_macro_benchmark(
            "veb_union_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "union",
            density,
            1,
            [&] {
                veb_result_tree = veb_tree1 | veb_tree2;
                ankerl::nanobench::doNotOptimizeAway(&veb_result_tree);
            },
            [&] { return veb_result_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_union_bench, g_output);

        auto veb_intersection_bench = run_macro_benchmark(
            "veb_intersection_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "intersection",
            density,
            1,
            [&] {
                veb_result_tree = veb_tree1 & veb_tree2;
                ankerl::nanobench::doNotOptimizeAway(&veb_result_tree);
            },
            [&] { return veb_result_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_intersection_bench, g_output);

        auto veb_xor_bench = run_macro_benchmark(
            "veb_xor_" + std::to_string(static_cast<int>(density * 100)),
            "vebitset",
            "xor",
            density,
            1,
            [&] {
                veb_result_tree = veb_tree1 ^ veb_tree2;
                ankerl::nanobench::doNotOptimizeAway(&veb_result_tree);
            },
            [&] { return veb_result_tree.get_allocated_bytes(); });
        ankerl::nanobench::render(benchmark_json_template(), veb_xor_bench, g_output);

        auto roaring_union_bench = run_macro_benchmark(
            "roaring_union_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "union",
            density,
            1,
            [&] {
                roaring_result_tree = roaring_tree1 | roaring_tree2;
                ankerl::nanobench::doNotOptimizeAway(&roaring_result_tree);
            },
            [&] { return roaring_result_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_union_bench, g_output);

        auto roaring_intersection_bench = run_macro_benchmark(
            "roaring_intersection_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "intersection",
            density,
            1,
            [&] {
                roaring_result_tree = roaring_tree1 & roaring_tree2;
                ankerl::nanobench::doNotOptimizeAway(&roaring_result_tree);
            },
            [&] { return roaring_result_tree.getSizeInBytes(true); });

        ankerl::nanobench::render(benchmark_json_template(), roaring_intersection_bench, g_output);

        auto roaring_xor_bench = run_macro_benchmark(
            "roaring_xor_" + std::to_string(static_cast<int>(density * 100)),
            "roaring",
            "xor",
            density,
            1,
            [&] {
                roaring_result_tree = roaring_tree1 ^ roaring_tree2;
                ankerl::nanobench::doNotOptimizeAway(&roaring_result_tree);
            },
            [&] { return roaring_result_tree.getSizeInBytes(true); });
        ankerl::nanobench::render(benchmark_json_template(), roaring_xor_bench, g_output);
    }
}
