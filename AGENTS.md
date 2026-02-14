# Agent Guidelines for vebitset

**Purpose**
- Guidance for agent contributors working on the vebitset repository
- Explains repository structure, build/test commands, and agent instructions

**Repository Context**
- **Language**: C++23 (core library) and C11 (C API)
- **Build System**: CMake 3.16+
- **Primary Locations**:
  - [include/vebitset.h](include/vebitset.h): Public C API
  - [src/VEB/VebTree.hpp](src/VEB/VebTree.hpp): Core VEB tree implementation
  - [tests/unit/](tests/unit/): C++ unit tests (doctest)
  - [tests/benchmarks/](tests/benchmarks/): Micro and macro benchmarks (nanobench)

**Key Files**
- [CMakeLists.txt](CMakeLists.txt): Root build configuration
- [include/vebitset.h](include/vebitset.h): Public C API with full function declarations
- [src/vebitset.cpp](src/vebitset.cpp): C API implementation
- [src/VEB/VebTree.hpp](src/VEB/VebTree.hpp): Core VEB tree (non-template variant-based)
- [src/VEB/node8.hpp](src/VEB/node8.hpp): 256-bit leaf nodes with SIMD optimization
- [src/VEB/node16.hpp](src/VEB/node16.hpp): Intermediate nodes (up to 2^16 elements)
- [src/VEB/node32.hpp](src/VEB/node32.hpp): Intermediate nodes (up to 2^32 elements)
- [src/VEB/node64.hpp](src/VEB/node64.hpp): Top-level nodes (up to 2^63 elements)
- [tests/unit/](tests/unit/): Unit test files (test_*.cpp)
- [tests/benchmarks/](tests/benchmarks/): Benchmark implementations (bench_*.cpp)

## How to Build & Test (Quick Reference)

### Build
```bash
# Release build (default, optimized)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Debug build
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug -j$(nproc)

# Build without tests
cmake -B build -DVEBITSET_BUILD_TESTS=OFF
cmake --build build

# Build with benchmarks enabled
cmake -B build -DVEBITSET_BUILD_BENCHMARKS=ON
cmake --build build
```

### Test
```bash
# Run all unit tests
cd build
ctest --verbose

# Run specific test by name
ctest --verbose --tests-regex test_basics

# Direct test executable
./build/tests/vebitset_tests

# Run single test file from tests/unit/
ctest --verbose --tests-regex "test_node_transitions"
```

### Benchmarks
```bash
# Build with benchmarks
cmake -B build -DVEBITSET_BUILD_BENCHMARKS=ON
cmake --build build

# Run all benchmarks (micro + macro)
./build/tests/benchmarks/vebitset_bench --output results.json

# Microbenchmarks only
./build/tests/benchmarks/vebitset_bench --micro-only

# Macrobenchmarks only (comparison with roaring, vector<bool>)
./build/tests/benchmarks/vebitset_bench --macro-only

# View results
cat results.json | jq '.'
```

## Agent (You) — Working Rules and Instructions

### General Workflow
1. **Always start with a todo** using the TodoWrite tool to track changes
2. **Before modifying files**, read them first to understand context and conventions
3. **Keep changes minimal and focused** on the requested task
4. **After edits, run relevant tests** immediately to validate changes
5. **If tests fail**, attempt to fix the root cause (limit 3 edit-test cycles per file)
6. **Do not commit** unless explicitly asked by the user

### Build & Test Protocol
- After file modifications, run the smallest test set that validates the change:
  ```bash
  cmake --build build && cd build && ctest --verbose --tests-regex relevant_test
  ```
- Prefer Release builds for performance validation
- Use Debug builds for diagnosing issues with detailed logging
- All tests must pass before considering a task complete

### Code Style & Standards
- **Language**: C++23 for core, C11 for API
- **Naming**: snake_case for all functions, methods, and variables
  - Example: `insert_value`, `successor_index`, `cluster_data_`
- **No Unnecessary Comments**: Code must be self-documenting
  - Comments only for complex algorithmic logic or non-obvious optimizations
- **No Implicit Conversions**:
  - Conversions must be non-narrowing (uint8_t → uint16_t is OK)
  - No implicit conversions to bool in conditions: use `if (ptr == nullptr)` not `if (!ptr)`
- **Brace Style**: Never elide braces in compound expressions
  ```cpp
  // WRONG: if (condition) statement;
  // RIGHT:
  if (condition) {
      statement;
  }
  ```

### Memory Management
- **VEB Nodes Must Be Destroyed**: All `Node16`, `Node32`, `Node64` instances must call `.destroy(alloc)` before going out of scope
  - Failure causes memory leaks and debug assertion failures
  - Exception: nodes stored within parent nodes (cleanup is parent's responsibility)
- **Manual Allocation**: Use `malloc`/`free` for C API, avoid C++ `new`/`delete`
- **Allocator Integration**: Use the provided allocator interface for consistency

### Performance Optimization
- **Memory efficiency is mission-critical**: Optimize for space first, then time
- **SIMD Usage**: Node8 must use xsimd for 256-bit operations (4 × uint64_t)
- **Flexible Array Members**: Use FAM for variable-length cluster storage in Node16
- **Zero-Copy**: Avoid unnecessary allocations in hot paths
- **Targeted Optimization**: Profile before optimizing; focus on O(log log U) operations

## VEB Node Invariants

### Min/Max Ownership
- `min` and `max` must be stored in the node itself, never delegated to clusters
- If removing an element would invalidate `min`/`max`, promote a value from clusters
- In-node `min`/`max` values are the authoritative single source of truth

### Node Semantics
- **Nodes are never empty** (except during destruction): always contain at least `min`/`max`
- **Summary is authoritative**: if `summary` contains a key, the cluster logically exists
- **Summary must be non-empty**: treat `summary` as a proper node with valid `min`/`max`
- **Clusterless ≠ empty**: a node with no clusters still has valid `min`/`max`

### Node Type Specifics
- **Node8**: 256-bit packed array (4 × uint64_t), SIMD-optimized, acts as a leaf
- **Node16**: Manages up to 2^16 elements via packed FAM of Node8 clusters
- **Node32**: Manages up to 2^32 elements via HashSet of Node16 clusters (keys inlined)
- **Node64**: Manages up to 2^63 elements via HashMap of Node32 clusters

## Testing Strategy

### Test Organization
- **Unit Tests**: [tests/unit/](tests/unit/) with doctest framework
- **Test Coverage**: Basics, queries, set operations, edge cases, serialization, node transitions, memory, fuzzing, complex scenarios
- **Microbenchmarks**: nanobench for individual operation profiling
- **Macrobenchmarks**: Compare against std::vector<bool> and roaring-bitmap

### Running Tests
```bash
# All tests
ctest --verbose

# Specific test file
ctest --verbose --tests-regex test_node_transitions

# Specific doctest
./build/tests/vebitset_tests "[test_basics]"
```

### Test Failures
1. Identify which test fails
2. Check the assertion message for root cause
3. Read the test code and implementation to understand the discrepancy
4. Make minimal fix and re-run only that test
5. Verify no regression with full test suite

## Documentation

### Code Documentation
- [README.md](README.md): User-facing library documentation
- [AGENTS.md](AGENTS.md): This file—developer/agent guidance
- [tests/README.md](tests/README.md): Test framework details

### Updating Docs
- After significant feature additions, update [README.md](README.md)
- Update [AGENTS.md](AGENTS.md) if agent workflow changes
- Document new benchmark scenarios in [tests/benchmarks/](tests/benchmarks/)

## Common Commands

### Building
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j$(nproc)
```

### Testing (Full)
```bash
cd build && ctest --verbose
```

### Testing (Specific)
```bash
cd build && ctest --verbose --tests-regex test_name
```

### Cleaning
```bash
rm -rf build build-debug
```

### Benchmarking
```bash
cmake -B build -DVEBITSET_BUILD_BENCHMARKS=ON
cmake --build build
./build/tests/benchmarks/vebitset_bench --output results.json
cat results.json | jq '.'
```

## Architecture Overview

vebitset uses a hierarchical VEB tree with automatic node type selection:

```
Insert value 5000000:
  → Exceeds Node16 limit (2^16)
  → Use Node32 (up to 2^32)
  → Allocate Node32 with sub-clusters of Node16
  → Each Node16 manages Node8 clusters (256-bit leaves)
```

**Node Stack**:
- **Node8**: 256-bit bitset (leaf)
- **Node16**: ≤2^16 bits, clusters of Node8
- **Node32**: ≤2^32 bits, clusters of Node16
- **Node64**: ≤2^63 bits, clusters of Node32

## Troubleshooting

### Build Failures
- Check C++23 compiler support (GCC 14+, Clang 17+)
- Verify CMake version ≥ 3.16
- Ensure xsimd is fetched (check `build/_deps`)

### Test Failures
- Run with `ctest --verbose` for detailed output
- Check memory with `valgrind` for leaks
- Enable debug logging in test code temporarily

### Performance Issues
- Profile with `perf` or comparable tool
- Use micro benchmarks to isolate bottlenecks
- Check node type selections for given data patterns
