import redis
import time
from prettytable import PrettyTable
import random

# --- CONFIGURATION ---
r = redis.Redis(decode_responses=True)
DATA_FILE_1 = 'tests/benchmarks/files/benchmark_{}_data_1.txt'
DATA_FILE_2 = 'tests/benchmarks/files/benchmark_{}_data_2.txt'
KEYS = {
    'sparse1': 'sparse1', 'dense1': 'dense1',
    'sparse2': 'sparse2', 'dense2': 'dense2',
    'compressed1': 'compressed1', 'compressed2': 'compressed2',
    'dest_s': 'dest_sparse', 'dest_d': 'dest_dense',
    'dest_c': 'dest_compressed',
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
    table.field_names = ["Operation", "SparseBitset", "Compressed", "Redis Bitmap", "μs/call (Sparse)", "μs/call (Compressed)", "μs/call (Dense)", "Correct"]
    
    # --- WRITE (INSERT / SETBIT) ---
    print("Benchmarking writes...")
    with r.pipeline() as pipe:
        for val in data1: pipe.execute_command('BITS.INSERT', KEYS['sparse1'], val)
        start_time = time.time(); pipe.execute(); s_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in data1: pipe.execute_command('R.SETBIT', KEYS['compressed1'], val, 1)
        start_time = time.time(); pipe.execute(); c_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in data1: pipe.setbit(KEYS['dense1'], val, 1)
        start_time = time.time(); pipe.execute(); d_time = time.time() - start_time
    table.add_row(["Insert 1M", f"{s_time:.2f}s", f"{c_time:.2f}s", f"{d_time:.2f}s", get_stats('bits.insert'), get_stats('R.SETBIT'), get_stats('setbit'), "N/A"])

    # --- COUNT ---
    s_count = int(r.execute_command('BITS.COUNT', KEYS['sparse1']))
    c_count = int(r.execute_command('R.BITCOUNT', KEYS['compressed1']))
    d_count = r.bitcount(KEYS['dense1'])
    table.add_row(["Count", f"{s_count}", f"{c_count}", f"{d_count}", get_stats('bits.count'), get_stats('R.BITCOUNT'), get_stats('bitcount'), compare_results(s_count, d_count)])
    
    # --- READ (GET / GETBIT) ---
    print("Benchmarking reads...")
    sample = random.sample(data1, 10000)
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('BITS.GET', KEYS['sparse1'], val)
        start_time = time.time(); pipe.execute(); s_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.getbit(KEYS['dense1'], val)
        start_time = time.time(); pipe.execute(); d_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('R.GETBIT', KEYS['compressed1'], val)
        start_time = time.time(); pipe.execute(); c_time = time.time() - start_time
    table.add_row(["Get (10k)", f"{s_time:.2f}s", f"{c_time:.2f}s", f"{d_time:.2f}s", get_stats('bits.get'), get_stats('R.GETBIT'), get_stats('getbit'), "N/A"])
    
    # --- REMOVE (REMOVE / SETBIT) ---
    print("Benchmarking removes...")
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('BITS.REMOVE', KEYS['sparse1'], val)
        start_time = time.time(); pipe.execute(); s_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('R.setbit', KEYS['compressed1'], val, 0)
        start_time = time.time(); pipe.execute(); c_time = time.time() - start_time
    with r.pipeline() as pipe:
        for val in sample: pipe.setbit(KEYS['dense1'], val, 0)
        start_time = time.time(); pipe.execute(); d_time = time.time() - start_time
    table.add_row(["Remove (10k)", f"{s_time:.2f}s", f"{c_time:.2f}s", f"{d_time:.2f}s", get_stats('bits.remove'), get_stats('R.SETBIT'), get_stats('setbit'), compare_results(int(r.execute_command('BITS.COUNT', KEYS['sparse1'])), r.bitcount(KEYS['dense1']))])

    # Re-insert removed data for subsequent tests
    with r.pipeline() as pipe:
        for val in sample: pipe.execute_command('BITS.INSERT', KEYS['sparse1'], val)
        pipe.execute()
    
    # --- MIN/MAX/SUCCESSOR/PREDECESSOR ---
    print("Benchmarking min/max/succ/pred...")
    min_val, max_val = int(r.execute_command('BITS.MIN', KEYS['sparse1'])), int(r.execute_command('BITS.MAX', KEYS['sparse1']))
    # succ = int(r.execute_command('BITS.SUCCESSOR', KEYS['sparse1'], min_val))
    # pred = int(r.execute_command('BITS.PREDECESSOR', KEYS['sparse1'], max_val))
    c_min = int(r.execute_command('R.MIN', KEYS['compressed1']))
    c_max = int(r.execute_command('R.MAX', KEYS['compressed1']))
    table.add_row(["Min/Max", f"{min_val}/{max_val}", f"{c_min}/{c_max}", "N/A", f"{get_stats('bits.min')}/{get_stats('bits.max')}", f"{get_stats('R.MIN')}/{get_stats('R.MAX')}", "N/A", f"{compare_results(min_val, c_min)}/{compare_results(max_val, c_max)}"])
    # table.add_row(["Succ/Pred", f"{succ}/{pred}", "N/A", f"{get_stats('bits.successor')}/{get_stats('bits.predecessor')}", "N/A", "N/A"])

    # --- SET OPERATIONS ---
    print("Benchmarking set operations...")
    # Load second dataset
    with r.pipeline() as pipe:
        for val in data2: pipe.execute_command('BITS.INSERT', KEYS['sparse2'], val)
        for val in data2: pipe.setbit(KEYS['dense2'], val, 1)
        for val in data2: pipe.execute_command('R.SETBIT', KEYS['compressed2'], val, 1)
        pipe.execute()
    
    # OR
    s_or_size = int(r.execute_command('BITS.OR', KEYS['dest_s'], KEYS['sparse1'], KEYS['sparse2']))
    r.execute_command('R.BITOP', 'OR', KEYS['dest_c'], KEYS['compressed1'], KEYS['compressed2'])
    r.bitop('OR', KEYS['dest_d'], KEYS['dense1'], KEYS['dense2'])
    d_or_size = r.bitcount(KEYS['dest_d'])
    c_or_size = r.execute_command('R.BITCOUNT', KEYS['dest_c'])
    table.add_row(["OR", f"{s_or_size}", f"{c_or_size}", f"{d_or_size}", get_stats('bits.or'), get_stats('R.BITOP'), get_stats('bitop'), compare_results(s_or_size, d_or_size)])
    
    # AND
    s_and_size = int(r.execute_command('BITS.AND', KEYS['dest_s'], KEYS['sparse1'], KEYS['sparse2']))
    r.execute_command('R.BITOP', 'AND', KEYS['dest_c'], KEYS['compressed1'], KEYS['compressed2'])
    r.bitop('AND', KEYS['dest_d'], KEYS['dense1'], KEYS['dense2'])
    d_and_size = r.bitcount(KEYS['dest_d'])
    c_and_size = r.execute_command('R.BITCOUNT', KEYS['dest_c'])
    table.add_row(["AND", f"{s_and_size}", f"{c_and_size}", f"{d_and_size}", get_stats('bits.and'), get_stats('R.BITOP'), get_stats('bitop'), compare_results(s_and_size, d_and_size)])

    # XOR
    s_xor_size = int(r.execute_command('BITS.XOR', KEYS['dest_s'], KEYS['sparse1'], KEYS['sparse2']))
    r.execute_command('R.BITOP', 'XOR', KEYS['dest_c'], KEYS['compressed1'], KEYS['compressed2'])
    r.bitop('XOR', KEYS['dest_d'], KEYS['dense1'], KEYS['dense2'])
    d_xor_size = r.bitcount(KEYS['dest_d'])
    c_xor_size = r.execute_command('R.BITCOUNT', KEYS['dest_c'])
    table.add_row(["XOR", f"{s_xor_size}", f"{c_xor_size}", f"{d_xor_size}", get_stats('bits.xor'), get_stats('R.BITOP'), get_stats('bitop'), compare_results(s_xor_size, d_xor_size)])

    # # --- TOARRAY ---
    # print("Benchmarking toarray...")
    # start_time = time.time()
    # s_array = r.execute_command('BITS.TOARRAY', KEYS['sparse1'])
    # s_toarray_time = time.time() - start_time
    # table.add_row(["ToArray", f"{s_toarray_time:.2f}s ({len(s_array)} items)", "N/A", get_stats('bits.toarray'), "N/A", "N/A"])

    # --- MEMORY USAGE ---
    s_mem1 = r.memory_usage(KEYS['sparse1']); s_mem2 = r.memory_usage(KEYS['sparse2'])
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
    main() 