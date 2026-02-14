#include "benchmark.hpp"
#include <roaring/roaring.hh>
#include <nanobench.h>
#include <iostream>

void bench_roaring_micro() {
    std::cout << "[Roaring Microbenchmarks]\n";
    
    const uint32_t LARGE_UNIVERSE = 10000000;
    const uint32_t SMALL_UNIVERSE = 10000;
    ankerl::nanobench::Rng rng;

    {
        roaring::Roaring rb;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("Roaring Insert (sparse)", [&]() {
            uint32_t val = rng.bounded(LARGE_UNIVERSE);
            rb.add(val);
            ankerl::nanobench::doNotOptimizeAway(&rb);
        });
    }

    {
        roaring::Roaring rb;
        uint32_t counter = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("Roaring Insert (sequential)", [&]() {
            uint32_t val = (counter++ % LARGE_UNIVERSE);
            rb.add(val);
            ankerl::nanobench::doNotOptimizeAway(&rb);
        });
    }

    {
        roaring::Roaring rb;
        uint32_t counter = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("Roaring Insert (small universe)", [&]() {
            uint32_t val = (counter++ % SMALL_UNIVERSE);
            rb.add(val);
            ankerl::nanobench::doNotOptimizeAway(&rb);
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 10000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(10000).run("Roaring Contains", [&]() {
            uint32_t val = rng.bounded(LARGE_UNIVERSE);
            ankerl::nanobench::doNotOptimizeAway(rb.contains(val));
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 10000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        auto values_to_delete = generate_random_values(5000, LARGE_UNIVERSE);
        uint32_t idx = 0;
        ankerl::nanobench::Bench().minEpochIterations(1000).run("Roaring Remove", [&]() {
            uint32_t val = values_to_delete[idx++ % values_to_delete.size()];
            rb.remove(val);
            ankerl::nanobench::doNotOptimizeAway(&rb);
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 10000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100000).run("Roaring Min", [&]() {
            ankerl::nanobench::doNotOptimizeAway(rb.minimum());
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 10000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100000).run("Roaring Max", [&]() {
            ankerl::nanobench::doNotOptimizeAway(rb.maximum());
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 10000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(10000).run("Roaring Count", [&]() {
            ankerl::nanobench::doNotOptimizeAway(rb.cardinality());
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 5000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("Roaring Iteration (5K elements)", [&]() {
            uint32_t count = 0;
            for (auto _ : rb) {
                count++;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }

    {
        roaring::Roaring rb;
        for (uint32_t i = 0; i < 100000; ++i) {
            rb.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(10).run("Roaring Iteration (100K elements)", [&]() {
            uint32_t count = 0;
            for (auto _ : rb) {
                count++;
            }
            ankerl::nanobench::doNotOptimizeAway(count);
        });
    }

    {
        roaring::Roaring rb1, rb2;
        for (uint32_t i = 0; i < LARGE_UNIVERSE * 0.95; ++i) {
            rb1.add(rng.bounded(LARGE_UNIVERSE));
            rb2.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("Roaring Union", [&]() {
            ankerl::nanobench::doNotOptimizeAway(rb1 |= rb2);
        });
    }

    {
        roaring::Roaring rb1, rb2;
        for (uint32_t i = 0; i < LARGE_UNIVERSE * 0.95; ++i) {
            rb1.add(rng.bounded(LARGE_UNIVERSE));
            rb2.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("Roaring Intersection", [&]() {
            ankerl::nanobench::doNotOptimizeAway(rb1 &= rb2);
        });
    }

    {
        roaring::Roaring rb1, rb2;
        for (uint32_t i = 0; i < LARGE_UNIVERSE * 0.95; ++i) {
            rb1.add(rng.bounded(LARGE_UNIVERSE));
            rb2.add(rng.bounded(LARGE_UNIVERSE));
        }
        ankerl::nanobench::Bench().minEpochIterations(100).run("Roaring XOR", [&]() {
            ankerl::nanobench::doNotOptimizeAway(rb1 ^= rb2);
        });
    }
}
