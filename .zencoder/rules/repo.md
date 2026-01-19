---
description: Repository Information Overview
alwaysApply: true
---

# SparseBit Information

## Summary
SparseBit is a Redis module that provides efficient sparse bitset operations using van Emde Boas (VEB) trees. It allows for high-performance manipulation of large bitsets with significant gaps between set bits, offering O(log log U) time complexity for most operations.

## Structure
- **src/**: Contains the main Redis module implementation (`bitset_module.c`) and Redis module header.
- **src/VEB/**: Houses the underlying C++ van Emde Boas tree library.
- **tests/flow/**: Python-based functional and integration tests using `RLTest`.
- **tests/benchmarks/**: Performance comparison scripts against native Redis bitmaps.
- **pack/**: RAMP configuration for packaging the module for Redis.

## Language & Runtime
**Language**: C11, C++23  
**Version**: Compatible with Redis 8.2+  
**Build System**: Makefile (root level), CMake (for VEB library)  
**Package Manager**: None (manual build or RAMP for Redis distribution)

## Dependencies
**Main Dependencies**:
- **Redis Server**: Required for loading and running the module.
- **libvebtree**: Internal C++ library built from `src/VEB`.

**Development Dependencies**:
- **GCC/G++**: With C++23 and AVX2 support.
- **CMake**: 3.16+ required for VEB library build.
- **Python 3**: For running tests and benchmarks.
- **RLTest**: Python framework for Redis module testing.
- **redis-py**: Redis Python client.

## Build & Installation
```bash
# Build the module (Release configuration by default)
make

# Build with debug symbols
make debug

# Install/Load in Redis
redis-server --loadmodule ./bitset.so
```

## Testing

**Framework**: RLTest (Python)  
**Test Location**: `tests/flow/`  
**Naming Convention**: `test_*.py`  
**Configuration**: Handled by `run_flow_tests.sh`

**Run Command**:
```bash
# Run all functional tests (creates venv and installs dependencies automatically)
./run_flow_tests.sh

# Run benchmarks (requires running Redis with module loaded)
python3 tests/benchmarks/generate_data.py
python3 tests/benchmarks/run_benchmarks.py
```
