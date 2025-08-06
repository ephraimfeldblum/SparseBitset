# Benchmark Results

## TL;DR

This benchmark compares three bitset implementations under varying data densities (0.05 to 0.95):

1. **SparseBitset**: A custom implementation, likely leveraging a sparse data structure.
2. **Compressed**: A Redis module-based implementation (`R.*` commands) that appears to be a compressed bitmap.
3. **Redis Bitmap**: The native Redis bitmap implementation.

### Key Findings:

**Memory Efficiency:**
- At lower densities (`< 0.4`), both SparseBitset and Compressed implementations are significantly more memory-efficient than the native Redis Bitmap.
- As density increases (`> 0.4`), SparseBitset becomes the most memory-efficient solution, outperforming both the Compressed and native Redis Bitmap implementations.

**Performance:**
- **Writes/Reads/Removes**: All implementations show excellent and comparable performance for individual element operations.
- **Count**: SparseBitset is slower at low densities but improves significantly as density increases.
- **Set Operations (OR/AND/XOR)**: SparseBitset is considerably slower than the bitmap-based solutions across all densities.
- **Min/Max**: SparseBitset and Compressed provide fast MIN/MAX operations, a feature not available in native Redis Bitmaps.

### Conclusion:

- Choose **SparseBitset** when memory efficiency is the top priority, especially for datasets that are not uniformly dense. It also provides useful MIN/MAX commands.
- Choose **Redis Bitmap** or the **Compressed** implementation when the primary requirement is high-speed set operations, and the higher memory cost for sparse data is acceptable.

---

## Detailed Results


Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

### Benchmark Results for 0.05 density

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
|---|---|---|---|---|---|---|---|
| Insert 1M | 1.29s | 1.35s | 1.36s | 0.17 | 0.36 | 0.07 | N/A |
| Count | 200000 | 200000 | 200000 | 72.00 | 4.00 | 198.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.49 | 0.78 | 0.25 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.06s | 0.52 | 0.39 | 0.08 | ✅ |
| Min/Max | 1/3999999 | 1/3999999 | N/A | 8.00/3.00 | 4.00/3.00 | N/A | ✅/✅ |
| OR | 389974 | 389974 | 389974 | 29678.00 | 870.00 | 57.00 | ✅ |
| AND | 10026 | 10026 | 10026 | 21669.00 | 1323.50 | 153.00 | ✅ |
| XOR | 379948 | 379948 | 379948 | 17785.00 | 1311.00 | 127.67 | ✅ |
| Memory (1) | 506032 B | 400545 B | 917560 B | N/A | N/A | N/A | N/A |
| Memory (2) | 506032 B | 400545 B | 589880 B | N/A | N/A | N/A | N/A |
-------------------------

Benchmark 1 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

### Benchmark Results for 0.1 density

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
|---|---|---|---|---|---|---|---|
| Insert 1M | 1.22s | 1.32s | 1.32s | 0.16 | 0.29 | 0.07 | N/A |
| Count | 200000 | 200000 | 200000 | 47.00 | 5.00 | 69.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.55 | 0.59 | 0.29 | N/A |
| Remove (10k) | 0.06s | 0.06s | 0.06s | 0.47 | 0.31 | 0.08 | ✅ |
| Min/Max | 1/1999999 | 1/1999999 | N/A | 7.00/2.00 | 4.00/2.00 | N/A | ✅/✅ |
| OR | 380157 | 380157 | 380157 | 27356.00 | 75.00 | 108.00 | ✅ |
| AND | 19843 | 19843 | 19843 | 16447.00 | 216.50 | 69.00 | ✅ |
| XOR | 360314 | 360314 | 360314 | 14325.00 | 167.33 | 60.67 | ✅ |
| Memory (1) | 253552 B | 252783 B | 524344 B | N/A | N/A | N/A | N/A |
| Memory (2) | 253552 B | 252989 B | 426040 B | N/A | N/A | N/A | N/A |
-------------------------

Benchmark 2 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

### Benchmark Results for 0.15 density

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
|---|---|---|---|---|---|---|---|
| Insert 1M | 1.21s | 1.32s | 1.32s | 0.15 | 0.25 | 0.07 | N/A |
| Count | 200000 | 200000 | 200000 | 85.00 | 15.00 | 47.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.75 | 0.50 | 0.24 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.60 | 0.28 | 0.07 | ✅ |
| Min/Max | 1/1333332 | 1/1333332 | N/A | 8.00/3.00 | 4.00/3.00 | N/A | ✅/✅ |
| OR | 370087 | 370087 | 370087 | 32578.00 | 50.00 | 24.00 | ✅ |
| AND | 29913 | 29913 | 29913 | 20458.00 | 233.50 | 53.00 | ✅ |
| XOR | 340174 | 340174 | 340174 | 12933.00 | 171.67 | 42.33 | ✅ |
| Memory (1) | 168912 B | 170743 B | 294968 B | N/A | N/A | N/A | N/A |
| Memory (2) | 168912 B | 170753 B | 360504 B | N/A | N/A | N/A | N/A |
-------------------------

Benchmark 3 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

### Benchmark Results for 0.2 density

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
|---|---|---|---|---|---|---|---|
| Insert 1M | 1.19s | 1.37s | 1.31s | 0.15 | 0.23 | 0.07 | N/A |
| Count | 200000 | 200000 | 200000 | 25.00 | 5.00 | 36.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.64 | 0.24 | 0.20 | N/A |
| Remove (10k) | 0.07s | 0.07s | 0.07s | 1.50 | 0.25 | 0.08 | ✅ |
| Min/Max | 0/999999 | 0/999999 | N/A | 8.00/3.00 | 4.00/3.00 | N/A | ✅/✅ |
| OR | 359780 | 359780 | 359780 | 25282.00 | 48.00 | 75.00 | ✅ |
| AND | 40220 | 40220 | 40220 | 23991.00 | 197.00 | 57.00 | ✅ |
| XOR | 319560 | 319560 | 319560 | 17108.00 | 146.00 | 45.67 | ✅ |
| Memory (1) | 126944 B | 129741 B | 131128 B | N/A | N/A | N/A | N/A |
| Memory (2) | 126944 B | 129695 B | 163896 B | N/A | N/A | N/A | N/A |
-------------------------

Benchmark 4 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.25 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.22s     |   1.33s    |    1.32s     |       0.16       |         0.22         |       0.06      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      21.00       |         5.00         |      39.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.07s     |       0.62       |         0.57         |       0.05      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.07s     |       0.97       |         0.24         |       0.07      |    ✅   |
|   Min/Max    |   0/799999   |  0/799999  |     N/A      |   1099.00/2.00   |      6.00/3.00       |       N/A       |  ✅/✅  |
|      OR      |    350150    |   350150   |    350150    |     25243.00     |        43.00         |      15.00      |    ✅   |
|     AND      |    49850     |   49850    |    49850     |     17021.00     |        131.00        |      24.50      |    ✅   |
|     XOR      |    300300    |   300300   |    300300    |     12546.00     |        99.33         |      20.67      |    ✅   |
|  Memory (1)  |   101648 B   |  105353 B  |   229432 B   |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   101648 B   |  105299 B  |   163896 B   |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 5 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.3 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.23s     |   1.31s    |    1.30s     |       0.15       |         0.21         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      20.00       |         4.00         |      25.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.42       |         0.51         |       0.16      |   N/A   |
| Remove (10k) |    0.06s     |   0.08s    |    0.07s     |       0.42       |         0.23         |       0.07      |    ✅   |
|   Min/Max    |   1/666665   |  1/666665  |     N/A      |    8.00/3.00     |      5.00/2.00       |       N/A       |  ✅/✅  |
|      OR      |    339808    |   339808   |    339808    |     25530.00     |        39.00         |      13.00      |    ✅   |
|     AND      |    60192     |   60192    |    60192     |     12838.00     |        39.50         |      92.00      |    ✅   |
|     XOR      |    279616    |   279616   |    279616    |     12602.00     |        37.33         |      65.00      |    ✅   |
|  Memory (1)  |   84624 B    |  88871 B   |   163896 B   |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   84624 B    |  88881 B   |   163896 B   |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 6 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.35 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.20s     |   1.31s    |    1.30s     |       0.15       |         0.20         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      17.00       |         4.00         |      21.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.68       |         0.51         |       0.17      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.07s     |       0.47       |         0.22         |       0.08      |    ✅   |
|   Min/Max    |   1/571427   |  1/571427  |     N/A      |    8.00/2.00     |      4.00/3.00       |       N/A       |  ✅/✅  |
|      OR      |    330166    |   330166   |    330166    |     25845.00     |        47.00         |      66.00      |    ✅   |
|     AND      |    69834     |   69834    |    69834     |     15933.00     |        33.00         |      115.00     |    ✅   |
|     XOR      |    260332    |   260332   |    260332    |     15812.00     |        28.00         |      80.67      |    ✅   |
|  Memory (1)  |   73968 B    |  73849 B   |   98360 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   73968 B    |  73849 B   |   163896 B   |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 7 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.4 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.19s     |   1.34s    |    1.33s     |       0.14       |         0.20         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      15.00       |         4.00         |      19.00      |    ✅   |
|  Get (10k)   |    0.07s     |   0.06s    |    0.06s     |       0.38       |         0.79         |       0.19      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.08s     |       0.45       |         0.23         |       0.07      |    ✅   |
|   Min/Max    |   1/499999   |  1/499999  |     N/A      |    10.00/2.00    |      5.00/2.00       |       N/A       |  ✅/✅  |
|      OR      |    319832    |   319832   |    319832    |     25601.00     |        22.00         |      17.00      |    ✅   |
|     AND      |    80168     |   80168    |    80168     |     14596.00     |        17.00         |      82.50      |    ✅   |
|     XOR      |    239664    |   239664   |    239664    |     15754.00     |        15.00         |      58.00      |    ✅   |
|  Memory (1)  |   64192 B    |  65649 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   64192 B    |  65649 B   |   131128 B   |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 8 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.45 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.18s     |   1.32s    |    1.31s     |       0.15       |         0.20         |       0.06      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      14.00       |         4.00         |      23.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.39       |         0.59         |       0.18      |   N/A   |
| Remove (10k) |    0.06s     |   0.06s    |    0.06s     |       0.46       |         0.21         |       0.07      |    ✅   |
|   Min/Max    |   1/444443   |  1/444443  |     N/A      |    7.00/2.00     |      4.00/2.00       |       N/A       |  ✅/✅  |
|      OR      |    310248    |   310248   |    310248    |     27250.00     |        19.00         |      10.00      |    ✅   |
|     AND      |    89752     |   89752    |    89752     |     20061.00     |        17.00         |      61.50      |    ✅   |
|     XOR      |    220496    |   220496   |    220496    |     19277.00     |        15.67         |      44.00      |    ✅   |
|  Memory (1)  |   57424 B    |  57449 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   57424 B    |  57449 B   |   98360 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 9 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.5 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.21s     |   1.32s    |    1.31s     |       0.14       |         0.19         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      15.00       |         4.00         |      15.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.41       |         0.47         |       0.15      |   N/A   |
| Remove (10k) |    0.06s     |   0.06s    |    0.06s     |       0.47       |         0.21         |       0.08      |    ✅   |
|   Min/Max    |   0/399999   |  0/399999  |     N/A      |    6.00/2.00     |      4.00/1.00       |       N/A       |  ✅/✅  |
|      OR      |    299841    |   299841   |    299841    |     29932.00     |        34.00         |      10.00      |    ✅   |
|     AND      |    100159    |   100159   |    100159    |     24179.00     |        39.00         |      14.50      |    ✅   |
|     XOR      |    199682    |   199682   |    199682    |     19077.00     |        39.67         |      13.00      |    ✅   |
|  Memory (1)  |   50960 B    |  56055 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   50960 B    |  56057 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 10 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.55 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.19s     |   1.32s    |    1.31s     |       0.15       |         0.20         |       0.06      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      13.00       |         4.00         |      14.00      |    ✅   |
|  Get (10k)   |    0.07s     |   0.06s    |    0.06s     |       1.10       |         0.62         |       0.26      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.06s     |       0.50       |         0.22         |       0.07      |    ✅   |
|   Min/Max    |   1/363635   |  1/363635  |     N/A      |    7.00/3.00     |      5.00/2.00       |       N/A       |  ✅/✅  |
|      OR      |    289843    |   289843   |    289843    |     29369.00     |        16.00         |       9.00      |    ✅   |
|     AND      |    110157    |   110157   |    110157    |     18312.00     |        14.50         |       9.00      |    ✅   |
|     XOR      |    179686    |   179686   |    179686    |     22367.00     |        20.67         |      10.33      |    ✅   |
|  Memory (1)  |   46464 B    |  49249 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   46464 B    |  49249 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 11 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.6 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.18s     |   1.33s    |    1.31s     |       0.15       |         0.19         |       0.06      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      17.00       |         5.00         |      21.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.08s     |       0.43       |         0.56         |       2.45      |   N/A   |
| Remove (10k) |    0.06s     |   0.06s    |    0.06s     |       0.66       |         0.22         |       0.07      |    ✅   |
|   Min/Max    |   0/333332   |  0/333332  |     N/A      |    6.00/3.00     |      4.00/2.00       |       N/A       |  ✅/✅  |
|      OR      |    280078    |   280078   |    280078    |     32712.00     |        31.00         |       8.00      |    ✅   |
|     AND      |    119922    |   119922   |    119922    |     15059.00     |        35.50         |      58.50      |    ✅   |
|     XOR      |    160156    |   160156   |    160156    |     13940.00     |        34.33         |      41.33      |    ✅   |
|  Memory (1)  |   42496 B    |  47851 B   |   98360 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   42496 B    |  47969 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 12 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.65 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.24s     |   1.32s    |    1.30s     |       0.15       |         0.19         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      12.00       |         4.00         |      12.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.40       |         0.48         |       0.17      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.07s     |       0.45       |         0.23         |       0.09      |    ✅   |
|   Min/Max    |   0/307691   |  0/307691  |     N/A      |    8.00/3.00     |      4.00/2.00       |       N/A       |  ✅/✅  |
|      OR      |    269889    |   269889   |    269889    |     32255.00     |        17.00         |       9.00      |    ✅   |
|     AND      |    130111    |   130111   |    130111    |     16930.00     |        16.50         |      11.00      |    ✅   |
|     XOR      |    139778    |   139778   |    139778    |     15808.00     |        14.33         |       9.67      |    ✅   |
|  Memory (1)  |   39328 B    |  41049 B   |   98360 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   39328 B    |  41049 B   |   98360 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 13 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.7 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.25s     |   1.32s    |    1.31s     |       0.15       |         0.19         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      15.00       |         9.00         |      11.00      |    ✅   |
|  Get (10k)   |    0.10s     |   0.06s    |    0.06s     |       1.05       |         0.47         |       0.16      |   N/A   |
| Remove (10k) |    0.06s     |   0.06s    |    0.07s     |       0.44       |         0.21         |       0.08      |    ✅   |
|   Min/Max    |   0/285713   |  0/285713  |     N/A      |    8.00/3.00     |      5.00/3.00       |       N/A       |  ✅/✅  |
|      OR      |    259997    |   259997   |    259997    |     25274.00     |        30.00         |      15.00      |    ✅   |
|     AND      |    140003    |   140003   |    140003    |     21587.00     |        21.50         |      101.00     |    ✅   |
|     XOR      |    119994    |   119994   |    119994    |     18724.00     |        18.00         |      70.00      |    ✅   |
|  Memory (1)  |   36416 B    |  41049 B   |   98360 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   36416 B    |  41049 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 14 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.75 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.17s     |   1.31s    |    1.38s     |       0.15       |         0.18         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      13.00       |         4.00         |      11.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.51       |         0.50         |       0.29      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.07s     |       0.57       |         0.21         |       0.07      |    ✅   |
|   Min/Max    |   0/266665   |  0/266665  |     N/A      |    10.00/7.00    |      6.00/4.00       |       N/A       |  ✅/✅  |
|      OR      |    250101    |   250101   |    250101    |     25213.00     |        33.00         |      11.00      |    ✅   |
|     AND      |    149899    |   149899   |    149899    |     12556.00     |        27.50         |      10.00      |    ✅   |
|     XOR      |    100202    |   100202   |    100202    |     11748.00     |        26.00         |       8.00      |    ✅   |
|  Memory (1)  |   34016 B    |  39629 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   34016 B    |  39573 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 15 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.8 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.23s     |   1.33s    |    1.42s     |       0.15       |         0.19         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      11.00       |         4.00         |      10.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.65       |         0.68         |       0.28      |   N/A   |
| Remove (10k) |    0.07s     |   0.08s    |    0.07s     |       1.83       |         0.22         |       0.07      |    ✅   |
|   Min/Max    |   0/249999   |  0/249999  |     N/A      |    11.00/2.00    |      5.00/3.00       |       N/A       |  ✅/✅  |
|      OR      |    239927    |   239927   |    239927    |     23538.00     |        30.00         |      16.00      |    ✅   |
|     AND      |    160073    |   160073   |    160073    |     16465.00     |        24.00         |      17.50      |    ✅   |
|     XOR      |    79854     |   79854    |    79854     |     11820.00     |        22.00         |      15.33      |    ✅   |
|  Memory (1)  |   32560 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   32560 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 16 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.85 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.23s     |   1.35s    |    1.35s     |       0.15       |         0.19         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      17.00       |         8.00         |      124.00     |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.44       |         0.93         |       0.16      |   N/A   |
| Remove (10k) |    0.06s     |   0.06s    |    0.06s     |       0.55       |         0.22         |       0.08      |    ✅   |
|   Min/Max    |   0/235293   |  0/235293  |     N/A      |    9.00/2.00     |      4.00/6.00       |       N/A       |  ✅/✅  |
|      OR      |    230080    |   230080   |    230080    |     28136.00     |        17.00         |       8.00      |    ✅   |
|     AND      |    169920    |   169920   |    169920    |     11220.00     |        14.00         |       7.50      |    ✅   |
|     XOR      |    60160     |   60160    |    60160     |     14325.00     |        12.67         |       7.33      |    ✅   |
|  Memory (1)  |   31056 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   31056 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 17 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.9 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.19s     |   1.36s    |    1.41s     |       0.15       |         0.19         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      12.00       |         5.00         |      11.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.08s    |    0.06s     |       0.74       |         0.20         |       0.27      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.07s     |       0.67       |         0.21         |       0.08      |    ✅   |
|   Min/Max    |   0/222221   |  0/222221  |     N/A      |    7.00/3.00     |      5.00/3.00       |       N/A       |  ✅/✅  |
|      OR      |    219987    |   219987   |    219987    |     31079.00     |        14.00         |       7.00      |    ✅   |
|     AND      |    180013    |   180013   |    180013    |     17344.00     |        11.50         |       6.50      |    ✅   |
|     XOR      |    39974     |   39974    |    39974     |     16937.00     |        10.33         |       6.33      |    ✅   |
|  Memory (1)  |   28912 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   28912 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 18 complete
Clearing old keys and resetting command stats...
Benchmarking writes...
Benchmarking reads...
Benchmarking removes...
Benchmarking min/max/succ/pred...
Benchmarking set operations...

--- BENCHMARK RESULTS for 0.95 density ---
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Operation   | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
|  Insert 1M   |    1.18s     |   1.35s    |    1.34s     |       0.15       |         0.18         |       0.07      |   N/A   |
|    Count     |    200000    |   200000   |    200000    |      13.00       |         8.00         |      13.00      |    ✅   |
|  Get (10k)   |    0.06s     |   0.06s    |    0.06s     |       0.42       |         0.75         |       0.17      |   N/A   |
| Remove (10k) |    0.06s     |   0.07s    |    0.06s     |       0.64       |         0.21         |       0.08      |    ✅   |
|   Min/Max    |   0/210525   |  0/210525  |     N/A      |    8.00/4.00     |      5.00/4.00       |       N/A       |  ✅/✅  |
|      OR      |    209996    |   209996   |    209996    |     35946.00     |        36.00         |      12.00      |    ✅   |
|     AND      |    190004    |   190004   |    190004    |     14457.00     |        25.50         |      10.50      |    ✅   |
|     XOR      |    19992     |   19992    |    19992     |     13699.00     |        23.00         |       9.33      |    ✅   |
|  Memory (1)  |   27056 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
|  Memory (2)  |   27056 B    |  32849 B   |   65592 B    |       N/A        |         N/A          |       N/A       |   N/A   |
+--------------+--------------+------------+--------------+------------------+----------------------+-----------------+---------+
-------------------------

Benchmark 19 complete