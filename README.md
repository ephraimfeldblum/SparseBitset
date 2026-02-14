# vebitset: A High-Performance C++ Bitset Library

A modern C++ library providing efficient bitset operations using van Emde Boas (VEB) trees. vebitset offers excellent performance for bitsets with a clean C API.

## Features

- **Fast operations**: O(log log U) time complexity for most operations, where U is the universe size
- **Memory efficient**: Only stores set bits with O(n) space complexity, where n is the number of set bits
- **Set operations**: Union, intersection, symmetric difference (XOR)
- **Range queries**: Count elements in ranges, find successors/predecessors
- **Serialization**: Serialize and deserialize bitsets to/from binary format
- **C API**: Clean C interface for easy integration with C code
- **C++ API**: Direct access to VEB tree implementation for maximum performance

## Building

### Prerequisites

- GCC/Clang with C++23 support
- CMake 3.16 or later
- xsimd (for SIMD optimizations, included via FetchContent)

### Build Steps

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j$(nproc)
```

### Build Options

```bash
# Debug build
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug

# Release build (default, optimized)
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build

# Build without tests
cmake -B build -DVEBITSET_BUILD_TESTS=OFF
cmake --build build

# Build with benchmarks
cmake -B build -DVEBITSET_BUILD_BENCHMARKS=ON
cmake --build build
```

## Testing

### Run All Tests

```bash
cd build/tests
ctest --verbose
```

### Run Specific Test

```bash
ctest --verbose --tests-regex test_basics
```

### Direct Test Execution

```bash
./build/tests/vebitset_tests
```

## Benchmarking

vebitset includes both microbenchmarks (using nanobench) and macrobenchmarks comparing against:
- std::vector<bool>
- Roaring Bitmap library

### Build and Run Benchmarks

```bash
cmake -B build -DVEBITSET_BUILD_BENCHMARKS=ON
cmake --build build
./build/tests/benchmarks/vebitset_bench --output results.json
```

### Benchmark Options

```bash
# Run all benchmarks
./build/tests/benchmarks/vebitset_bench --output results.json

# Microbenchmarks only
./build/tests/benchmarks/vebitset_bench --micro-only

# Macrobenchmarks only
./build/tests/benchmarks/vebitset_bench --macro-only
```

## C API Usage

### Creating and Destroying Bitsets

```c
#include <vebitset.h>

// Create a new bitset
vebitset_t bitset = vebitset_create();

// Use the bitset...

// Destroy when done
vebitset_destroy(bitset);
```

### Basic Operations

```c
// Insert elements
vebitset_insert(bitset, 5);
vebitset_insert(bitset, 10);
vebitset_insert(bitset, 100);

// Check membership
bool exists = vebitset_contains(bitset, 10);

// Remove elements
vebitset_remove(bitset, 5);

// Clear all elements
vebitset_clear(bitset);
```

### Query Operations

```c
// Get min/max elements
uint64_t min = vebitset_min(bitset);
uint64_t max = vebitset_max(bitset);

// Count elements
uint64_t count = vebitset_count(bitset);

// Count elements in a range
uint64_t range_count = vebitset_count_range(bitset, 0, 1000);

// Find successor/predecessor
uint64_t succ = vebitset_successor(bitset, 100);
uint64_t pred = vebitset_predecessor(bitset, 100);

// Get all elements as array
uint64_t* array = NULL;
uint64_t size = 0;
vebitset_to_array(bitset, &array, &size);
free(array);
```

### Set Operations

```c
// Create two bitsets for operations
vebitset_t set1 = vebitset_create();
vebitset_t set2 = vebitset_create();

// Populate with elements...

// Union
vebitset_t result = vebitset_union(set1, set2);

// Intersection
vebitset_t result = vebitset_intersection(set1, set2);

// Symmetric difference (XOR)
vebitset_t result = vebitset_symmetric_difference(set1, set2);

// Cleanup
vebitset_destroy(set1);
vebitset_destroy(set2);
vebitset_destroy(result);
```

### Serialization

```c
// Serialize to bytes
uint8_t* buffer = NULL;
uint64_t size = 0;
vebitset_serialize(bitset, &buffer, &size);

// Deserialize from bytes
vebitset_t restored = vebitset_deserialize(buffer, size);
free(buffer);
```

## C++ API

For C++ code, you can use the VEB tree implementation directly:

```cpp
#include "VEB/VebTree.hpp"

VebTree tree;
tree.insert(5);
tree.insert(10);

bool has = tree.contains(10);
uint64_t count = tree.count();
tree.remove(5);
```

## Performance Characteristics

- **Insert**: O(log log U) amortized
- **Remove**: O(log log U) amortized
- **Contains**: O(log log U)
- **Min/Max**: O(1)
- **Successor/Predecessor**: O(log log U)
- **Count**: O(log log U) for range counts
- **Set operations**: O(n + m) for inputs of size n and m
- **Memory**: O(n) for n set bits

## Architecture

vebitset uses a hierarchical van Emde Boas tree structure:

- **Node8** (256-bit leaf): Direct bitset with SIMD optimizations
- **Node16**: Manages up to 2^16 elements via clusters of Node8
- **Node32**: Manages up to 2^32 elements via clusters of Node16
- **Node64**: Manages up to 2^63 elements via clusters of Node32

The structure automatically selects the appropriate node type based on the data range.

## Documentation

- [AGENTS.md](AGENTS.md) - Developer guide for contributors and agents
- [tests/README.md](tests/README.md) - Test framework documentation

## License

MIT License

## Implementation

This project demonstrates advanced C++ programming techniques including:
- Modern C++23 features
- Memory-efficient data structures
- SIMD optimization with xsimd
- Zero-copy serialization
- Flexible Array Members (FAM) for compact storage
