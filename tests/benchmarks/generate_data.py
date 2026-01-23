#!/usr/bin/env python3

import random
import os
import argparse

def generate_benchmark_data(filename, count, universe):
    """
    Generates a file with a list of random integers for benchmarking.

    Args:
        filename (str): The name of the file to create.
        count (int): The number of integers to generate.
        universe (int): The maximum value for the random integers.
    """
    print(f"Generating {count} unique numbers in a universe of {universe}...")
    with open(filename, 'w') as f:
        data = set()

        # Ensure min (0) and max (universe - 1) are in the data
        if universe > 0:
            data.add(1)
        if universe > 1:
            data.add(universe - 1)

        while len(data) < count:
            data.add(random.randint(0, universe - 1))
        
        # Convert set to a list and shuffle it to ensure random order in the file
        data_list = list(data)
        random.shuffle(data_list)
        
        for item in data_list:
            f.write(f"{item}\n")
    print(f"Data generated in {filename}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate benchmark data for veb bitsets.")
    parser.add_argument('--count', type=int, default=1_000_000, help='Number of elements to generate.')
    args = parser.parse_args()

    # Calculate universe size based on density
    # density = count / universe_size  => universe_size = count / density
    for i in range(1, 20):
        universe_size = int(args.count / (i/20))

        # Ensure benchmarks directory exists
        benchmarks_dir = 'tests/benchmarks/files'
        if not os.path.exists(benchmarks_dir):
            os.makedirs(benchmarks_dir)

        # Generate data for two keys for set operations
        generate_benchmark_data(os.path.join(benchmarks_dir, f'benchmark_{i}_data_1.txt'), args.count, universe_size)
        generate_benchmark_data(os.path.join(benchmarks_dir, f'benchmark_{i}_data_2.txt'), args.count, universe_size)