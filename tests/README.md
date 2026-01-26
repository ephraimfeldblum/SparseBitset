# Testing Guide

This document provides instructions on how to run the flow tests and the performance benchmarks for the Redis bitset module.

## Flow Tests

The flow tests are designed to verify the correctness and functionality of the module's commands under various Redis configurations (e.g., with AOF and replication).

### Running the Flow Tests

For convenience, a make recipe `make test`, is provided to automate the entire process. It will:

1.  Create a Python virtual environment (`.venv_rltest`) if it doesn't exist.
2.  Install the necessary dependencies (`RLTest`, `redis`).
3.  Build the `bitset.so` module if it's missing.
4.  Run the test suite with different Redis configurations.

To run the flow tests, simply execute the script from the project's root directory:

```bash
make test
```

## Benchmark Tests

The benchmark tests are designed to compare the performance and memory usage of the custom bitset module against Redis's native bitmap commands.

### 1. Generate Benchmark Data

The `tests/benchmarks/generate_data.py` script creates the data files required for the benchmarks. You can control the number of elements and the data density.

**Usage:**

```bash
python3 tests/benchmarks/generate_data.py [--count COUNT] [--density DENSITY]
```

**Arguments:**

*   `--count`: The number of elements to generate (default: 1,000,000).
*   `--density`: The data density, which determines the universe size (`universe = count / density`). A lower density creates a sparser dataset. Default is `0.01` (1%).

**Examples:**

*   Generate 1 million elements with 30% density:
    ```bash
    python3 tests/benchmarks/generate_data.py --count 1000000 --density 0.3
    ```

*   Generate 500,000 elements with 1% density (default):
    ```bash
    python3 tests/benchmarks/generate_data.py --count 500000
    ```

### 2. Run the Benchmarks

Before running the benchmarks, ensure your Redis server is running and the `bitset.so` module is loaded.

**Start Redis with the module:**

```bash
redis-server --loadmodule ./bitset.so
```

**Run the benchmark script:**

```bash
python3 tests/benchmarks/run_benchmarks.py
```

The script will:

1.  Clear any old benchmark data from Redis.
2.  Load the generated data into both the custom bitset and a native Redis bitmap.
3.  Run a series of benchmarks comparing all major commands (`INSERT`, `GET`, `REMOVE`, set operations, etc.).
4.  Fetch server-side latency for each command using `COMMANDSTATS`.
5.  Perform correctness checks to ensure the module's results match the native commands.
6.  Print a detailed comparison table. 