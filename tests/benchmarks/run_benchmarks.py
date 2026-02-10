#!/usr/bin/env python3

import redis
import time
from prettytable import PrettyTable
import argparse
import random

# --- CONFIGURATION ---
r = None
DATA_FILE_1 = 'tests/benchmarks/files/benchmark_{}_data_1.txt'
DATA_FILE_2 = 'tests/benchmarks/files/benchmark_{}_data_2.txt'
KEYS = {
    'veb1': 'veb1', 'dense1': 'dense1',
    'veb2': 'veb2', 'dense2': 'dense2',
    'compressed1': 'compressed1', 'compressed2': 'compressed2',
    's_or': 'veb_union', 's_and': 'veb_inter', 's_xor': 'veb_diff',
    'd_or': 'dense_union', 'd_and': 'dense_inter', 'd_xor': 'dense_diff',
    'c_or': 'compressed_union', 'c_and': 'compressed_inter', 'c_xor': 'compressed_diff',
}

# --- HELPER FUNCTIONS ---
def load_data(filename):
    with open(filename, 'r') as f:
        return [int(line.strip()) for line in f]

def clear_and_reset():
    print("Clearing old keys and resetting command stats...")
    r.flushall()
    r.config_resetstat()

def get_stats(cmd):
    stats = r.info('commandstats').get(f'cmdstat_{cmd}')
    return f"{stats['usec_per_call']:.2f}" if stats else "N/A"

def compare_results(a, b):
    return "✅" if a == b else "❌"

# --- BENCHMARK FUNCTIONS ---
def run_all_benchmarks(data1, data2, i):
    table = PrettyTable()
    table.field_names = ["Operation", "vEB Bitmap", "Roaring Bitmap", "Redis Bitmap", "μs/call (vEB)", "μs/call (Roaring)", "μs/call (Redis)", "Correct"]
    
    # --- WRITE (INSERT / SETBIT) ---
    print("Benchmarking writes...")
    with r.pipeline() as pipe:
        for val in data1: pipe.setbit(KEYS['dense1'], val, 1)
        start_time = time.time(); pipe.execute(); d_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in data1: pipe.execute_command('R.SETBIT', KEYS['compressed1'], val, 1)
        start_time = time.time(); pipe.execute(); c_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in data1: pipe.execute_command('BITS.INSERT', KEYS['veb1'], val)
        start_time = time.time(); pipe.execute(); s_time = time.time() - start_time
    table.add_row([f"Insert ({len(data1)})", f"{s_time:.2f}s", f"{c_time:.2f}s", f"{d_time:.2f}s", get_stats('bits.insert'), get_stats('R.SETBIT'), get_stats('setbit'), "N/A"])

    # --- COUNT ---
    d_count = r.bitcount(KEYS['dense1'])
    c_count = int(r.execute_command('R.BITCOUNT', KEYS['compressed1']))
    s_count = int(r.execute_command('BITS.COUNT', KEYS['veb1']))
    table.add_row(["Count", f"{s_count}", f"{c_count}", f"{d_count}", get_stats('bits.count'), get_stats('R.BITCOUNT'), get_stats('bitcount'), compare_results(s_count, d_count)])
    
    # --- READ (GET / GETBIT) ---
    print("Benchmarking reads...")
    n_sample = s_count // 100
    sample = random.sample(data1, n_sample)
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('R.GETBIT', KEYS['compressed1'], val)
        start_time = time.time(); pipe.execute(); c_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.getbit(KEYS['dense1'], val)
        start_time = time.time(); pipe.execute(); d_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('BITS.GET', KEYS['veb1'], val)
        start_time = time.time(); pipe.execute(); s_time = time.time() - start_time
    table.add_row([f"Get ({n_sample})", f"{s_time:.2f}s", f"{c_time:.2f}s", f"{d_time:.2f}s", get_stats('bits.get'), get_stats('R.GETBIT'), get_stats('getbit'), "N/A"])

    # --- REMOVE (REMOVE / SETBIT) ---
    print("Benchmarking removes...")
    with r.pipeline() as pipe:
        for val in sample: pipe.setbit(KEYS['dense1'], val, 0)
        start_time = time.time(); pipe.execute(); d_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('R.setbit', KEYS['compressed1'], val, 0)
        start_time = time.time(); pipe.execute(); c_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('BITS.REMOVE', KEYS['veb1'], val)
        start_time = time.time(); pipe.execute(); s_time = time.time() - start_time
    table.add_row([f"Remove ({n_sample})", f"{s_time:.2f}s", f"{c_time:.2f}s", f"{d_time:.2f}s", get_stats('bits.remove'), get_stats('R.SETBIT'), get_stats('setbit'), compare_results(int(r.execute_command('BITS.COUNT', KEYS['veb1'])), r.bitcount(KEYS['dense1']))])

    # Re-insert removed data for subsequent tests
    with r.pipeline() as pipe:
        for val in sample:
            pipe.execute_command('BITS.INSERT', KEYS['veb1'], val)
            pipe.execute_command('R.SETBIT', KEYS['compressed1'], val, 1)
            pipe.execute_command('SETBIT', KEYS['dense1'], val, 1)
        pipe.execute()
    
    # --- MIN/MAX ---
    # print("Benchmarking min/max...")
    # c_min, c_max = int(r.execute_command('R.MIN', KEYS['compressed1'])), int(r.execute_command('R.MAX', KEYS['compressed1']))
    # min_val, max_val = int(r.execute_command('BITS.MIN', KEYS['veb1'])), int(r.execute_command('BITS.MAX', KEYS['veb1']))
    # table.add_row(["Min/Max", f"{min_val}/{max_val}", f"{c_min}/{c_max}", "N/A", f"{get_stats('bits.min')}/{get_stats('bits.max')}", f"{get_stats('R.MIN')}/{get_stats('R.MAX')}", "N/A", f"{compare_results(min_val, c_min)}/{compare_results(max_val, c_max)}"])

    # --- ITERATION ---
    # print("Benchmarking iteration...")
    # with r.pipeline() as pipe:
    #     for val in data1: pipe.bitpos(KEYS['dense1'], 1, val + 1)
    #     start_time = time.time(); pipe.execute(); d_iter_time = time.time() - start_time
    # with r.pipeline() as pipe:
    #     for val in data1: pipe.execute_command('BITS.SUCCESSOR', KEYS['veb1'], val)
    #     start_time = time.time(); pipe.execute(); s_iter_time = time.time() - start_time
    # table.add_row(["Iteration", f"{s_iter_time:.2f}s", "N/A", f"{d_iter_time:.2f}s", get_stats('bits.successor'), "N/A", get_stats('bitpos'), "N/A"])

    # Load second dataset
    with r.pipeline() as pipe:
        for val in data2: pipe.execute_command('BITS.INSERT', KEYS['veb2'], val)
        for val in data2: pipe.setbit(KEYS['dense2'], val, 1)
        for val in data2: pipe.execute_command('R.SETBIT', KEYS['compressed2'], val, 1)
        pipe.execute()

    # --- SET OPERATIONS ---
    print("Benchmarking set operations...")

    # OR
    r.bitop('OR', KEYS['d_or'], KEYS['dense1'], KEYS['dense2'])
    r.execute_command('R.BITOP', 'OR', KEYS['c_or'], KEYS['compressed1'], KEYS['compressed2'])
    r.execute_command('BITS.OP', 'OR', KEYS['s_or'], KEYS['veb1'], KEYS['veb2'])
    s_or_size = r.execute_command('BITS.COUNT', KEYS['s_or'])
    d_or_size = r.bitcount(KEYS['d_or'])
    c_or_size = r.execute_command('R.BITCOUNT', KEYS['c_or'])
    table.add_row(["OR", f"{s_or_size}", f"{c_or_size}", f"{d_or_size}", get_stats('bits.op'), get_stats('R.BITOP'), get_stats('bitop'), compare_results(s_or_size, d_or_size)])

    # AND
    r.bitop('AND', KEYS['d_and'], KEYS['dense1'], KEYS['dense2'])
    r.execute_command('R.BITOP', 'AND', KEYS['c_and'], KEYS['compressed1'], KEYS['compressed2'])
    r.execute_command('BITS.OP', 'AND', KEYS['s_and'], KEYS['veb1'], KEYS['veb2'])
    s_and_size = r.execute_command('BITS.COUNT', KEYS['s_and'])
    d_and_size = r.bitcount(KEYS['d_and'])
    c_and_size = r.execute_command('R.BITCOUNT', KEYS['c_and'])
    table.add_row(["AND", f"{s_and_size}", f"{c_and_size}", f"{d_and_size}", get_stats('bits.op'), get_stats('R.BITOP'), get_stats('bitop'), compare_results(s_and_size, d_and_size)])

    # XOR
    r.bitop('XOR', KEYS['d_xor'], KEYS['dense1'], KEYS['dense2'])
    r.execute_command('R.BITOP', 'XOR', KEYS['c_xor'], KEYS['compressed1'], KEYS['compressed2'])
    r.execute_command('BITS.OP', 'XOR', KEYS['s_xor'], KEYS['veb1'], KEYS['veb2'])
    s_xor_size = r.execute_command('BITS.COUNT', KEYS['s_xor'])
    d_xor_size = r.bitcount(KEYS['d_xor'])
    c_xor_size = r.execute_command('R.BITCOUNT', KEYS['c_xor'])
    table.add_row(["XOR", f"{s_xor_size}", f"{c_xor_size}", f"{d_xor_size}", get_stats('bits.op'), get_stats('R.BITOP'), get_stats('bitop'), compare_results(s_xor_size, d_xor_size)])

    # # --- TOARRAY ---
    # print("Benchmarking toarray...")
    # start_time = time.time()
    # s_array = r.execute_command('BITS.TOARRAY', KEYS['veb1'])
    # s_toarray_time = time.time() - start_time
    # table.add_row(["ToArray", f"{s_toarray_time:.2f}s ({len(s_array)} items)", "N/A", get_stats('bits.toarray'), "N/A", "N/A"])

    # --- MEMORY USAGE ---
    s_mem1 = r.memory_usage(KEYS['veb1']); s_mem2 = r.memory_usage(KEYS['veb2'])
    d_mem1 = r.memory_usage(KEYS['dense1']); d_mem2 = r.memory_usage(KEYS['dense2'])
    c_mem1 = r.memory_usage(KEYS['compressed1']); c_mem2 = r.memory_usage(KEYS['compressed2'])
    table.add_row(["Memory (1)", f"{s_mem1} B", f"{c_mem1} B", f"{d_mem1} B", "N/A", "N/A", "N/A", "N/A"])
    table.add_row(["Memory (2)", f"{s_mem2} B", f"{c_mem2} B", f"{d_mem2} B", "N/A", "N/A", "N/A", "N/A"])

    
    print(f"\n--- BENCHMARK RESULTS for {i/20} density ---")
    print(table)
    print("-------------------------\n")

def main():
    for i in range(1, 20):
        clear_and_reset()
        data1 = load_data(DATA_FILE_1.format(i))
        data2 = load_data(DATA_FILE_2.format(i))
        run_all_benchmarks(data1, data2, i)
        print(f"Benchmark {i} complete")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Run benchmarks for vEB bitsets against Redis bitmaps and Roaring bitmaps.")
    parser.add_argument('--port', type=int, default=6379, help='Redis server port.')
    args = parser.parse_args()
    r = redis.Redis(decode_responses=True, port=args.port)
    main()
