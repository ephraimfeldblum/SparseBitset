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
        while len(data) < count:
            data.add(random.randint(0, universe - 1))
        
        for item in data:
            f.write(f"{item}\n")
    print(f"Data generated in {filename}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate benchmark data for sparse bitsets.")
    parser.add_argument('--density', type=float, default=0.01, help='Data density (e.g., 0.01 for 1%%). Lower is sparser.')
    parser.add_argument('--count', type=int, default=1_000_000, help='Number of elements to generate.')
    args = parser.parse_args()

    # Calculate universe size based on density
    # density = count / universe_size  => universe_size = count / density
    universe_size = int(args.count / args.density)

    # Ensure benchmarks directory exists
    benchmarks_dir = 'tests/benchmarks'
    if not os.path.exists(benchmarks_dir):
        os.makedirs(benchmarks_dir)
        
    # Generate data for two keys for set operations
    generate_benchmark_data(os.path.join(benchmarks_dir, 'benchmark_data_1.txt'), args.count, universe_size)
    generate_benchmark_data(os.path.join(benchmarks_dir, 'benchmark_data_2.txt'), args.count, universe_size) 