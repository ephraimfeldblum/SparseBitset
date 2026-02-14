#pragma once

#include <nanobench.h>
#include <chrono>
#include <string>
#include <vector>
#include <iomanip>
#include <fstream>
#include <sstream>

inline std::vector<uint32_t> generate_random_values(uint32_t count, uint32_t max_value) {
    std::vector<uint32_t> values;
    values.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        values.push_back(ankerl::nanobench::Rng{}.bounded(max_value));
    }
    return values;
}

inline char const* benchmark_json_template() noexcept {
    return R"({{#result}}{{^-first}},{{/-first}}{
  "name": "{{name}}",
  "density": "{{context(density)}}",
  "library": "{{context(library)}}",
  "scenario": "{{context(scenario)}}",
  "operations": {{context(operations)}},
  "median_ns": {{median(elapsed)}},
  "average_ns": {{average(elapsed)}},
  "stddev_pct": {{medianAbsolutePercentError(elapsed)}},
  "total_ns": {{sumProduct(iterations, elapsed)}},
  "iterations": {{sum(iterations)}}
},
{{/result}})";
}

template <typename Fn1, typename Fn2>
ankerl::nanobench::Bench run_macro_benchmark(
    const std::string& name,
    const std::string& library,
    const std::string& scenario,
    double density,
    uint64_t element_count,
    Fn1&& benchmark_fn,
    [[maybe_unused]] Fn2&& memory_fn) {
    return ankerl::nanobench::Bench()
        .name(name)
        .context("library", library)
        .context("scenario", scenario)
        .context("density", std::to_string(density))
        .context("operations", std::to_string(element_count))
        .minEpochIterations(10)
        .epochs(5)
        .output(nullptr)
        .run(name, benchmark_fn);
}
