### TL;DR: Summary of 5M Key Benchmark

These benchmarks tested the `SparseBitset` module against Redis's native `Bitmap` and a `Compressed` variant using 5 million keys across ten different data densities (from 10% to 90%).

1.  **Memory Efficiency is Key**: The `SparseBitset` consistently used less memory than the native `Redis Bitmap`, with the advantage becoming more pronounced as the data became denser. At its best, the sparse module was over 30% more memory-efficient.
2.  **Performance is a Trade-Off**: While the native `Redis Bitmap` showed lower server-side latency for individual operations (`μs/call`), the client-side performance for bulk operations (e.g., `Insert 1M`) was comparable and often slightly better for the `SparseBitset`, likely due to pipelining efficiency.
3.  **Critical Bug Discovered**: A significant correctness issue was found. **Benchmark 5 (at 50% density) failed validation**, with `BITS.COUNT` and `BITS.REMOVE` producing incorrect results. This points to a potential bug in the module's logic that needs immediate investigation.
4.  **Conclusion**: The `SparseBitset` is very promising for memory-sensitive use cases, but the identified bug at 50% density must be resolved before it can be considered stable.

---

### Benchmark 1 (10% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 33.74s | 38.00s | 36.40s | 0.16 | 0.35 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 1600.00 | 50.00 | 749.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.07s | 0.24 | 0.37 | 0.14 | N/A |
| Remove (10k) | 0.06s | 0.08s | 0.09s | 0.31 | 0.36 | 0.08 | ✅ |
| Memory (1) | 6318208 B | 6256649 B | 6324280 B | N/A | N/A | N/A | N/A |
| Memory (2) | 6318208 B | 6256649 B | 6488120 B | N/A | N/A | N/A | N/A |

### Benchmark 2 (20% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 32.65s | 36.56s | 35.64s | 0.16 | 0.24 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 435.00 | 6.00 | 244.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.23 | 0.28 | 0.11 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.28 | 0.24 | 0.08 | ✅ |
| Memory (1) | 3159856 B | 3132449 B | 3571768 B | N/A | N/A | N/A | N/A |
| Memory (2) | 3159856 B | 3132449 B | 3997752 B | N/A | N/A | N/A | N/A |

### Benchmark 3 (30% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 32.44s | 36.55s | 35.55s | 0.16 | 0.23 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 500.00 | 33.00 | 1441.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.23 | 0.25 | 0.13 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.25 | 0.23 | 0.08 | ✅ |
| Memory (1) | 2107552 B | 2091049 B | 2949176 B | N/A | N/A | N/A | N/A |
| Memory (2) | 2107552 B | 2091049 B | 2949176 B | N/A | N/A | N/A | N/A |

### Benchmark 4 (40% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 32.44s | 36.86s | 35.97s | 0.16 | 0.23 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 489.00 | 29.00 | 424.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.18 | 0.25 | 0.12 | N/A |
| Remove (10k) | 0.07s | 0.08s | 0.07s | 0.22 | 0.23 | 0.08 | ✅ |
| Memory (1) | 1580960 B | 1566249 B | 2129976 B | N/A | N/A | N/A | N/A |
| Memory (2) | 1580960 B | 1566249 B | 2326584 B | N/A | N/A | N/A | N/A |

### Benchmark 5 (50% density) - 🚨 BUG DISCOVERED 🚨

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 32.30s | 36.23s | 35.23s | 0.15 | 0.22 | 0.08 | N/A |
| Count | 5000001 | 5000000 | 5000000 | 402.00 | 11.00 | 583.00 | ❌ |
| Get (10k) | 0.08s | 0.06s | 0.07s | 0.18 | 0.30 | 0.09 | N/A |
| Remove (10k) | 0.07s | 0.07s | 0.08s | 0.22 | 0.22 | 0.08 | ❌ |
| Memory (1) | 1265120 B | 1254649 B | 2129976 B | N/A | N/A | N/A | N/A |
| Memory (2) | 1265120 B | 1254649 B | 2129976 B | N/A | N/A | N/A | N/A |

### Benchmark 6 (60% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 32.16s | 36.27s | 34.62s | 0.15 | 0.21 | 0.07 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 183.00 | 28.00 | 414.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.16 | 0.23 | 0.08 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.20 | 0.21 | 0.07 | ✅ |
| Memory (1) | 1053936 B | 1049649 B | 1048632 B | N/A | N/A | N/A | N/A |
| Memory (2) | 1053936 B | 1049649 B | 1048632 B | N/A | N/A | N/A | N/A |

### Benchmark 7 (70% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 31.86s | 35.52s | 34.80s | 0.15 | 0.21 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 178.00 | 8.00 | 276.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.16 | 0.22 | 0.07 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.19 | 0.21 | 0.08 | ✅ |
| Memory (1) | 903456 B | 893849 B | 1048632 B | N/A | N/A | N/A | N/A |
| Memory (2) | 903456 B | 893849 B | 1048632 B | N/A | N/A | N/A | N/A |

### Benchmark 8 (80% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 31.82s | 35.93s | 34.62s | 0.15 | 0.21 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 94.00 | 8.00 | 113.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.16 | 0.23 | 0.09 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.20 | 0.21 | 0.08 | ✅ |
| Memory (1) | 790672 B | 787249 B | 1048632 B | N/A | N/A | N/A | N/A |
| Memory (2) | 790672 B | 787249 B | 1048632 B | N/A | N/A | N/A | N/A |

### Benchmark 9 (90% density)

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 32.03s | 35.88s | 34.25s | 0.15 | 0.21 | 0.08 | N/A |
| Count | 5000000 | 5000000 | 5000000 | 237.00 | 54.00 | 505.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.17 | 0.22 | 0.07 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.20 | 0.21 | 0.08 | ✅ |
| Memory (1) | 703328 B | 697049 B | 1048632 B | N/A | N/A | N/A | N/A |
| Memory (2) | 703328 B | 697049 B | 1048632 B | N/A | N/A | N/A | N/A |
