#include "benchmark.hpp"
#include "VEB/VebTree.hpp"
#include <nanobench.h>
#include <iostream>

void bench_veb_micro() {
    std::cout << "[VEB Microbenchmarks]\n";
    
    const uint32_t LARGE_UNIVERSE = 10000000;
    const uint32_t SMALL_UNIVERSE = 10000;
    ankerl::nanobench::Rng rng;

    {
        VebTree tree;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("VEB Insert (sparse)", [&]() {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
            ankerl::nanobench::doNotOptimizeAway(&tree);
        });
    }

    {
        VebTree tree;
        uint32_t counter = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("VEB Insert (sequential)", [&]() {
            tree.insert(counter++ % LARGE_UNIVERSE);
            ankerl::nanobench::doNotOptimizeAway(&tree);
        });
    }

    {
        VebTree tree;
        uint32_t counter = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("VEB Insert (small universe)", [&]() {
            tree.insert(counter++ % SMALL_UNIVERSE);
            ankerl::nanobench::doNotOptimizeAway(&tree);
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(10000).run("VEB Contains", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree.contains(rng.bounded(LARGE_UNIVERSE)));
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        auto values_to_delete = generate_random_values(5000, LARGE_UNIVERSE);
        uint32_t idx = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("VEB Remove", [&]() {
            tree.remove(values_to_delete[idx++ % values_to_delete.size()]);
            ankerl::nanobench::doNotOptimizeAway(&tree);
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100000).run("VEB Min", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree.min());
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100000).run("VEB Max", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree.max());
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(10000).run("VEB Size", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree.size());
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(1000).run("VEB Successor", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree.successor(rng.bounded(LARGE_UNIVERSE)));
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 10000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(1000).run("VEB Predecessor", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree.predecessor(rng.bounded(LARGE_UNIVERSE)));
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 5000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("VEB Iteration (5K elements)", [&]() {
            uint32_t count = 0;
            for (auto _ : tree) {
                count++;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }

    {
        VebTree tree;
        for (uint32_t i = 0; i < 100000; ++i) {
            tree.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(10).run("VEB Iteration (100K elements)", [&]() {
            uint32_t count = 0;
            for (auto _ : tree) {
                count++;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }

    {
        VebTree tree1, tree2;
        for (uint32_t i = 0; i < LARGE_UNIVERSE * 0.95; ++i) {
            tree1.insert(rng.bounded(LARGE_UNIVERSE));
            tree2.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("VEB Union", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree1 |= tree2);
        });
    }

    {
        VebTree tree1, tree2;
        for (uint32_t i = 0; i < LARGE_UNIVERSE * 0.95; ++i) {
            tree1.insert(rng.bounded(LARGE_UNIVERSE));
            tree2.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("VEB Intersection", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree1 &= tree2);
        });
    }

    {
        VebTree tree1, tree2;
        for (uint32_t i = 0; i < LARGE_UNIVERSE * 0.95; ++i) {
            tree1.insert(rng.bounded(LARGE_UNIVERSE));
            tree2.insert(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("VEB XOR", [&]() {
            ankerl::nanobench::doNotOptimizeAway(tree1 ^= tree2);
        });
    }
}
