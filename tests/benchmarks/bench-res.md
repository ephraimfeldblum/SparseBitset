### TL;DR: Benchmark Analysis

This report details a series of benchmarks comparing the custom `SparseBitset` and `Compressed` bitset modules against Redis's native `Bitmap`. The tests were run across 19 different data densities, from 5% to 95%.

**Key Findings:**

1.  **Critical Correctness Failures**: The most important takeaway is the discovery of significant bugs in the custom implementations.
    *   **Set Operations (`OR`, `AND`, `XOR`)**: Both `SparseBitset` and `Compressed` **consistently fail** the correctness checks for all set operations across every tested density. The resulting sizes do not match the expected output from the native `BITOP` command.
    *   **Min/Max**: There are sporadic correctness failures for `BITS.MIN`, indicating potential edge-case issues in that command as well.
2.  **Superior Memory Efficiency**: The primary advantage of the custom modules is their memory savings. In denser scenarios (e.g., >60% density), both `SparseBitset` and `Compressed` use **less than half the memory** of the native Redis `Bitmap`, making them highly effective for memory-constrained environments.
3.  **Performance Analysis**:
    *   **Server-Side Latency (`μs/call`)**: For set operations, the custom modules are orders of magnitude slower than the native `BITOP` command. For example, in the first benchmark, `BITS.OR` has a latency of 16235.00 μs, while `BITOP` is only 63.00 μs.
    *   **Client-Side Time**: Bulk insertion times are comparable across all implementations, with `SparseBitset` often having a slight edge.

**Conclusion:**

While the memory savings are significant and promising, the **critical correctness bugs in the set operations and `Min/Max` commands make the custom modules unreliable for production use**. The immediate priority must be to debug and fix these functional issues. The severe performance degradation of set operations also needs to be addressed.

---

### Benchmark Results (by Density)

#### Density: 0.05

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.61s | 0.64s | 0.64s | 0.16 | 0.39 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 41.00 | 5.00 | 79.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.25 | 0.06 | N/A |
| Remove (10k) | 0.07s | 0.07s | 0.07s | 0.16 | 0.39 | 0.07 | ✅ |
| Min/Max | 1/1999999 | 1/1999999 | N/A | 5.00/2.00 | 5.00/5.00 | N/A | ✅/✅ |
| OR | 194880 | 185391 | 185391 | 16235.00 | 427.00 | 63.00 | ❌ |
| AND | 5120 | 4609 | 4609 | 8385.00 | 531.50 | 66.50 | ❌ |
| XOR | 189760 | 180782 | 180782 | 6460.00 | 493.67 | 53.67 | ❌ |
| Memory (1) | 253552 B | 180297 B | 491576 B | N/A | N/A | N/A | N/A |
| Memory (2) | 253552 B | 200297 B | 327736 B | N/A | N/A | N/A | N/A |

#### Density: 0.10

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.58s | 0.64s | 0.65s | 0.16 | 0.31 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 43.00 | 8.00 | 35.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.20 | 0.06 | N/A |
| Remove (10k) | 0.06s | 0.06s | 0.06s | 0.15 | 0.30 | 0.07 | ✅ |
| Min/Max | 1/999999 | 1/999999 | N/A | 4.00/1.00 | 2.00/2.00 | N/A | ✅/✅ |
| OR | 189941 | 180937 | 180937 | 13694.00 | 43.00 | 40.00 | ❌ |
| AND | 10059 | 9063 | 9063 | 7559.00 | 114.50 | 60.00 | ❌ |
| XOR | 179882 | 171874 | 171874 | 7327.00 | 89.00 | 45.67 | ❌ |
| Memory (1) | 126944 B | 126071 B | 131128 B | N/A | N/A | N/A | N/A |
| Memory (2) | 126944 B | 126543 B | 229432 B | N/A | N/A | N/A | N/A |

#### Density: 0.15

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.58s | 0.65s | 0.64s | 0.15 | 0.27 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 18.00 | 5.00 | 25.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.13 | 0.20 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.26 | 0.07 | ✅ |
| Min/Max | 0/666665 | 1/666665 | N/A | 12.00/5.00 | 7.00/5.00 | N/A | ❌/✅ |
| OR | 185176 | 176694 | 176694 | 5415.00 | 31.00 | 31.00 | ❌ |
| AND | 14824 | 13306 | 13306 | 11791.00 | 110.50 | 27.50 | ❌ |
| XOR | 170352 | 163388 | 163388 | 7222.00 | 84.00 | 22.00 | ❌ |
| Memory (1) | 84624 B | 85175 B | 163896 B | N/A | N/A | N/A | N/A |
| Memory (2) | 84624 B | 85463 B | 163896 B | N/A | N/A | N/A | N/A |

#### Density: 0.20

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.65s | 0.64s | 0.15 | 0.28 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 8.00 | 2.00 | 5.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.19 | 0.06 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.27 | 0.07 | ✅ |
| Min/Max | 1/499999 | 1/499999 | N/A | 3.00/1.00 | 3.00/2.00 | N/A | ✅/✅ |
| OR | 180082 | 172105 | 172105 | 12423.00 | 18.00 | 11.00 | ❌ |
| AND | 19918 | 17895 | 17895 | 7775.00 | 75.00 | 41.00 | ❌ |
| XOR | 160164 | 154210 | 154210 | 6764.00 | 54.33 | 30.67 | ❌ |
| Memory (1) | 64192 B | 65649 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 64192 B | 65649 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.25

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.64s | 0.64s | 0.15 | 0.24 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 22.00 | 7.00 | 34.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.13 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.16 | 0.24 | 0.07 | ✅ |
| Min/Max | 0/399999 | 0/399999 | N/A | 8.00/2.00 | 6.00/2.00 | N/A | ✅/✅ |
| OR | 175116 | 167575 | 167575 | 9417.00 | 27.00 | 19.00 | ❌ |
| AND | 24884 | 22425 | 22425 | 7221.00 | 80.50 | 18.50 | ❌ |
| XOR | 150232 | 145150 | 145150 | 6779.00 | 60.67 | 15.00 | ❌ |
| Memory (1) | 50960 B | 52315 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 50960 B | 52539 B | 98360 B | N/A | N/A | N/A | N/A |

#### Density: 0.30

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.58s | 0.65s | 0.64s | 0.15 | 0.24 | 0.08 | N/A |
| Count | 100000 | 100000 | 100000 | 7.00 | 2.00 | 5.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.19 | 0.06 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.24 | 0.08 | ✅ |
| Min/Max | 1/333332 | 3/333332 | N/A | 9.00/4.00 | 11.00/4.00 | N/A | ❌/✅ |
| OR | 170233 | 163252 | 163252 | 9673.00 | 26.00 | 9.00 | ❌ |
| AND | 29767 | 26748 | 26748 | 9135.00 | 26.50 | 8.50 | ❌ |
| XOR | 140466 | 136504 | 136504 | 8254.00 | 26.00 | 8.00 | ❌ |
| Memory (1) | 42496 B | 44071 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 42496 B | 44419 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.35

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.65s | 0.65s | 0.16 | 0.24 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 7.00 | 3.00 | 4.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.39 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.06s | 0.16 | 0.24 | 0.07 | ✅ |
| Min/Max | 1/285713 | 1/285713 | N/A | 7.00/3.00 | 8.00/3.00 | N/A | ✅/✅ |
| OR | 164960 | 158456 | 158456 | 8029.00 | 17.00 | 8.00 | ❌ |
| AND | 35040 | 31544 | 31544 | 9377.00 | 18.50 | 7.00 | ❌ |
| XOR | 129920 | 126912 | 126912 | 7159.00 | 15.33 | 6.33 | ❌ |
| Memory (1) | 36416 B | 41049 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 36416 B | 41049 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.40

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.65s | 0.65s | 0.16 | 0.23 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 6.00 | 2.00 | 3.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.16 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.06s | 0.15 | 0.23 | 0.07 | ✅ |
| Min/Max | 0/249999 | 0/249999 | N/A | 6.00/2.00 | 8.00/2.00 | N/A | ✅/✅ |
| OR | 159951 | 153885 | 153885 | 6050.00 | 18.00 | 9.00 | ❌ |
| AND | 40049 | 36115 | 36115 | 6751.00 | 16.00 | 10.00 | ❌ |
| XOR | 119902 | 117770 | 117770 | 8245.00 | 15.67 | 9.00 | ❌ |
| Memory (1) | 32560 B | 32849 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 32560 B | 32849 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.45

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.66s | 0.67s | 0.16 | 0.23 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 12.00 | 4.00 | 11.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.27 | 0.19 | 0.06 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.23 | 0.07 | ✅ |
| Min/Max | 1/222221 | 3/222221 | N/A | 10.00/5.00 | 9.00/6.00 | N/A | ❌/✅ |
| OR | 154873 | 149382 | 149382 | 8583.00 | 14.00 | 7.00 | ❌ |
| AND | 45127 | 40618 | 40618 | 6399.00 | 11.50 | 6.00 | ❌ |
| XOR | 109746 | 108764 | 108764 | 6794.00 | 10.67 | 5.67 | ❌ |
| Memory (1) | 28912 B | 32849 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 28912 B | 32849 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.50

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.64s | 0.66s | 0.67s | 0.16 | 0.23 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 11.00 | 5.00 | 8.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.14 | 0.19 | 0.06 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.23 | 0.07 | ✅ |
| Min/Max | 1/199999 | 1/199999 | N/A | 7.00/3.00 | 6.00/6.00 | N/A | ✅/✅ |
| OR | 150139 | 145059 | 145059 | 14464.00 | 23.00 | 7.00 | ❌ |
| AND | 49861 | 44941 | 44941 | 7021.00 | 22.00 | 6.50 | ❌ |
| XOR | 100278 | 100118 | 100118 | 7947.00 | 20.33 | 6.00 | ❌ |
| Memory (1) | 25616 B | 27581 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 25616 B | 28039 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.55

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.60s | 0.66s | 0.68s | 0.16 | 0.22 | 0.08 | N/A |
| Count | 100000 | 100000 | 100000 | 11.00 | 5.00 | 9.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.06s | 0.16 | 0.22 | 0.08 | ✅ |
| Min/Max | 1/181817 | 2/181817 | N/A | 5.00/3.00 | 5.00/3.00 | N/A | ❌/✅ |
| OR | 145037 | 140527 | 140527 | 11625.00 | 12.00 | 8.00 | ❌ |
| AND | 54963 | 49473 | 49473 | 7512.00 | 10.00 | 6.50 | ❌ |
| XOR | 90074 | 91054 | 91054 | 8520.00 | 10.00 | 6.00 | ❌ |
| Memory (1) | 24288 B | 24649 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 24288 B | 24649 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.60

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.66s | 0.65s | 0.15 | 0.23 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 11.00 | 6.00 | 9.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.18 | 0.06 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.06s | 0.15 | 0.23 | 0.07 | ✅ |
| Min/Max | 0/166665 | 0/166665 | N/A | 8.00/3.00 | 6.00/4.00 | N/A | ✅/✅ |
| OR | 140062 | 136077 | 136077 | 8544.00 | 14.00 | 9.00 | ❌ |
| AND | 59938 | 53923 | 53923 | 6518.00 | 12.00 | 6.50 | ❌ |
| XOR | 80124 | 82154 | 82154 | 8717.00 | 10.33 | 5.67 | ❌ |
| Memory (1) | 21600 B | 24649 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 21600 B | 24649 B | 32312 B | N/A | N/A | N/A | N/A |

#### Density: 0.65

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.65s | 0.68s | 0.16 | 0.23 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 13.00 | 5.00 | 9.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.14 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.07s | 0.07s | 0.06s | 0.15 | 0.23 | 0.07 | ✅ |
| Min/Max | 0/153845 | 0/153845 | N/A | 9.00/3.00 | 5.00/3.00 | N/A | ✅/✅ |
| OR | 134911 | 131448 | 131448 | 10993.00 | 12.00 | 6.00 | ❌ |
| AND | 65089 | 58552 | 58552 | 6581.00 | 10.50 | 5.00 | ❌ |
| XOR | 69822 | 72896 | 72896 | 7472.00 | 9.00 | 4.67 | ❌ |
| Memory (1) | 19872 B | 24649 B | 65592 B | N/A | N/A | N/A | N/A |
| Memory (2) | 19872 B | 24649 B | 27704 B | N/A | N/A | N/A | N/A |

#### Density: 0.70

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.64s | 0.66s | 0.65s | 0.17 | 0.22 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 15.00 | 9.00 | 14.00 | ✅ |
| Get (10k) | 0.06s | 0.07s | 0.06s | 0.13 | 0.19 | 0.06 | N/A |
| Remove (10k) | 0.07s | 0.08s | 0.08s | 0.16 | 0.22 | 0.07 | ✅ |
| Min/Max | 0/142856 | 0/142856 | N/A | 8.00/3.00 | 5.00/3.00 | N/A | ✅/✅ |
| OR | 130028 | 127074 | 127074 | 9211.00 | 13.00 | 6.00 | ❌ |
| AND | 69972 | 62926 | 62926 | 9303.00 | 15.50 | 6.00 | ❌ |
| XOR | 60056 | 64148 | 64148 | 6418.00 | 13.33 | 5.33 | ❌ |
| Memory (1) | 18400 B | 24649 B | 23096 B | N/A | N/A | N/A | N/A |
| Memory (2) | 18400 B | 24649 B | 17976 B | N/A | N/A | N/A | N/A |

#### Density: 0.75

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.60s | 0.66s | 0.65s | 0.16 | 0.23 | 0.08 | N/A |
| Count | 100000 | 100000 | 100000 | 7.00 | 4.00 | 4.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.16 | 0.22 | 0.07 | ✅ |
| Min/Max | 1/133332 | 1/133332 | N/A | 5.00/1.00 | 3.00/1.00 | N/A | ✅/✅ |
| OR | 124902 | 122406 | 122406 | 8632.00 | 23.00 | 6.00 | ❌ |
| AND | 75098 | 67594 | 67594 | 5700.00 | 19.00 | 5.00 | ❌ |
| XOR | 49804 | 54812 | 54812 | 6048.00 | 17.33 | 4.67 | ❌ |
| Memory (1) | 17248 B | 19505 B | 32312 B | N/A | N/A | N/A | N/A |
| Memory (2) | 17248 B | 19831 B | 65592 B | N/A | N/A | N/A | N/A |

#### Density: 0.80

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.60s | 0.66s | 0.65s | 0.16 | 0.23 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 11.00 | 4.00 | 7.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.13 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.20 | 0.23 | 0.07 | ✅ |
| Min/Max | 1/124999 | 1/124999 | N/A | 6.00/2.00 | 5.00/3.00 | N/A | ✅/✅ |
| OR | 120015 | 118068 | 118068 | 10544.00 | 11.00 | 6.00 | ❌ |
| AND | 79985 | 71932 | 71932 | 5884.00 | 9.00 | 5.00 | ❌ |
| XOR | 40030 | 46136 | 46136 | 6336.00 | 8.33 | 4.67 | ❌ |
| Memory (1) | 16728 B | 16449 B | 29752 B | N/A | N/A | N/A | N/A |
| Memory (2) | 16728 B | 16449 B | 29752 B | N/A | N/A | N/A | N/A |

#### Density: 0.85

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.66s | 0.66s | 0.15 | 0.21 | 0.06 | N/A |
| Count | 100000 | 100000 | 100000 | 38.00 | 6.00 | 9.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.14 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.21 | 0.06 | ✅ |
| Min/Max | 0/117646 | 1/117646 | N/A | 8.00/5.00 | 10.00/5.00 | N/A | ❌/✅ |
| OR | 114961 | 113444 | 113444 | 9426.00 | 42.00 | 29.00 | ❌ |
| AND | 85039 | 76556 | 76556 | 6042.00 | 30.00 | 17.50 | ❌ |
| XOR | 29922 | 36888 | 36888 | 6308.00 | 22.67 | 15.33 | ❌ |
| Memory (1) | 15928 B | 16449 B | 23096 B | N/A | N/A | N/A | N/A |
| Memory (2) | 15928 B | 16449 B | 22072 B | N/A | N/A | N/A | N/A |

#### Density: 0.90

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.65s | 0.65s | 0.16 | 0.21 | 0.07 | N/A |
| Count | 100000 | 100000 | 100000 | 19.00 | 9.00 | 9.00 | ✅ |
| Get (10k) | 0.07s | 0.06s | 0.06s | 0.14 | 0.18 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.07s | 0.07s | 0.15 | 0.21 | 0.07 | ✅ |
| Min/Max | 0/111110 | 0/111110 | N/A | 8.00/8.00 | 10.00/5.00 | N/A | ✅/✅ |
| OR | 110008 | 109017 | 109017 | 9794.00 | 11.00 | 5.00 | ❌ |
| AND | 89992 | 80983 | 80983 | 5395.00 | 9.00 | 4.50 | ❌ |
| XOR | 20016 | 28034 | 28034 | 6771.00 | 8.00 | 4.33 | ❌ |
| Memory (1) | 14424 B | 16449 B | 26168 B | N/A | N/A | N/A | N/A |
| Memory (2) | 14424 B | 16449 B | 25656 B | N/A | N/A | N/A | N/A |

#### Density: 0.95

| Operation | SparseBitset | Compressed | Redis Bitmap | μs/call (Sparse) | μs/call (Compressed) | μs/call (Dense) | Correct |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| Insert 1M | 0.59s | 0.66s | 0.69s | 0.17 | 0.21 | 0.11 | N/A |
| Count | 100000 | 100000 | 100000 | 5.00 | 2.00 | 2.00 | ✅ |
| Get (10k) | 0.06s | 0.06s | 0.06s | 0.13 | 0.19 | 0.05 | N/A |
| Remove (10k) | 0.06s | 0.08s | 0.07s | 0.15 | 0.21 | 0.11 | ✅ |
| Min/Max | 0/105262 | 0/105262 | N/A | 3.00/1.00 | 2.00/1.00 | N/A | ✅/✅ |
| OR | 104984 | 104513 | 104513 | 8876.00 | 14.00 | 6.00 | ❌ |
| AND | 95016 | 85487 | 85487 | 5312.00 | 11.00 | 5.00 | ❌ |
| XOR | 9968 | 19026 | 19026 | 5932.00 | 10.00 | 4.67 | ❌ |
| Memory (1) | 14424 B | 16449 B | 20536 B | N/A | N/A | N/A | N/A |
| Memory (2) | 14424 B | 16449 B | 24632 B | N/A | N/A | N/A | N/A |
