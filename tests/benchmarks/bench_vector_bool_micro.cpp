#include "benchmark.hpp"
#include <nanobench.h>
#include <vector>
#include <algorithm>
#include <iostream>

void bench_vector_bool_micro() {
    std::cout << "[std::vector<bool> Microbenchmarks]\n";
    
    const uint32_t LARGE_UNIVERSE = 10000000;
    const uint32_t SMALL_UNIVERSE = 10000;
    ankerl::nanobench::Rng rng;

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        ankerl::nanobench::Bench().minEpochIterations(1000).run("vector<bool> Insert (random)", [&]() {
            uint32_t idx = rng.bounded(LARGE_UNIVERSE);
            vb[idx] = true;
            ankerl::nanobench::doNotOptimizeAway(&vb);
        });
    }

    {
        std::vector<bool> vb(SMALL_UNIVERSE, false);
        uint32_t counter = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("vector<bool> Insert (sequential, small)", [&]() {
            uint32_t idx = counter++ % SMALL_UNIVERSE;
            vb[idx] = true;
            ankerl::nanobench::doNotOptimizeAway(&vb);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 10000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(10000).run("vector<bool> Contains", [&]() {
            uint32_t idx = rng.bounded(LARGE_UNIVERSE);
            ankerl::nanobench::doNotOptimizeAway(vb[idx]);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 10000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(1000).run("vector<bool> Erase (random)", [&]() {
            uint32_t idx = rng.bounded(LARGE_UNIVERSE);
            vb[idx] = false;
            ankerl::nanobench::doNotOptimizeAway(&vb);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 10000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(10).run("vector<bool> Count", [&]() {
            uint32_t count = 0;
            for (bool b : vb) {
                count += b ? 1 : 0;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 10000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("vector<bool> Min", [&]() {
            auto it = std::find(vb.begin(), vb.end(), true);
            uint32_t idx = (it != vb.end()) ? static_cast<uint32_t>(std::distance(vb.begin(), it)) : UINT32_MAX;
            ankerl::nanobench::doNotOptimizeAway(idx);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 10000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("vector<bool> Max", [&]() {
            auto it = std::find(vb.rbegin(), vb.rend(), true);
            uint32_t idx = (it != vb.rend()) ? static_cast<uint32_t>(std::distance(vb.begin(), it.base()) - 1) : UINT32_MAX;
            ankerl::nanobench::doNotOptimizeAway(idx);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 5000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("vector<bool> Iteration (5K elements)", [&]() {
            uint32_t count = 0;
            for (uint32_t i = 0; i < vb.size(); ++i) {
                if (vb[i]) count++;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }

    {
        std::vector<bool> vb(LARGE_UNIVERSE, false);
        for (uint32_t i = 0; i < 100000; ++i) {
            vb[rng.bounded(LARGE_UNIVERSE)] = true;
        }
        ankerl::nanobench::Bench().minEpochIterations(10).run("vector<bool> Iteration (100K elements)", [&]() {
            uint32_t count = 0;
            for (uint32_t i = 0; i < vb.size(); ++i) {
                if (vb[i]) count++;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }
}
