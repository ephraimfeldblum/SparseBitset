# Redis Bitset Module

A Redis module that provides efficient sparse bitset operations using van Emde Boas (VEB) trees. This module allows you to store and manipulate large sparse bitsets with excellent performance characteristics.

## Features

- **Sparse bitset operations**: Efficiently handle bitsets with large gaps between set bits
- **VEB tree backend**: O(log log U) time complexity, where U is the universe size
- **Multiple hash table implementations**: Support for std, Abseil, and Boost hash tables
- **Set operations**: Union, intersection, symmetric difference (XOR)
- **Range queries**: Find all set bits in a given range
- **Memory efficient**: Only stores set bits, not the entire bit array, with O(n) space complexity, where n is the number of set bits

## Building

### Prerequisites

- GCC with C++23 support
- CMake 3.16 or later
- Redis server (for testing)
- Optional: Abseil or Boost libraries for alternative hash table implementations

### Build Steps

Simply run from the root directory:

```bash
make
```

This will:
- Automatically create the VEB build directory if it doesn't exist
- Configure and build the VEB library with Release optimization
- Build the Redis module (`bitset.so`)

### Build Options

- **`make`** or **`make release`** - Build with Release configuration (default, optimized)
- **`make debug`** - Build with Debug configuration (includes debug symbols)
- **`make clean`** - Clean all build artifacts including VEB library
- **`make help`** - Show available build targets and options

The build process automatically handles dependencies and only builds the necessary shared libraries.

## Installation

1. **Copy the module** to your Redis modules directory or keep it in the current directory
2. **Load the module at Redis startup** (preferred method):
   ```bash
   redis-server --loadmodule /path/to/bitset.so
   ```

   Or add to your `redis.conf`:
   ```
   loadmodule /path/to/bitset.so
   ```

3. **Alternative: Load the module in a running Redis instance**:
   ```bash
   redis-cli MODULE LOAD /path/to/bitset.so
   ```

## Commands

All commands use the `BITS.` prefix to avoid conflicts with Redis built-in commands.

### Basic Operations

- **`BITS.INSERT key element [element ...]`** - Add one or more elements to the bitset
  - Returns: Number of elements that were newly added
- **`BITS.REMOVE key element [element ...]`** - Remove one or more elements from the bitset
  - Returns: Number of elements that were removed
- **`BITS.SET key offset value`** - Sets or clears the bit at offset in the bitset
  - `offset` - The bit position (must be >= 0)
  - `value` - The bit value to set (0 or 1)
  - Returns: The previous value of the bit at offset
  - Behaves similarly to Redis SETBIT command
- **`BITS.CLEAR key`** - Remove all elements from the bitset
  - Returns: "OK"

### Query Operations

- **`BITS.GET key offset`** - Returns the bit value at offset in the bitset
  - Returns: 1 if the bit is set, 0 otherwise
  - Behaves similarly to Redis GETBIT command
- **`BITS.MIN key`** - Get the smallest element in the bitset
  - Returns: The minimum element, or null if empty
- **`BITS.MAX key`** - Get the largest element in the bitset
  - Returns: The maximum element, or null if empty
- **`BITS.SUCCESSOR key element`** - Find the smallest element greater than the given element
  - Returns: The successor element, or null if none exists
- **`BITS.PREDECESSOR key element`** - Find the largest element smaller than the given element
  - Returns: The predecessor element, or null if none exists
- **`BITS.COUNT key [start end [BYTE | BIT]]`** - Count elements in the bitset or within a range
  - `key` - The bitset key
  - `start` - Optional start index (default: 0)
  - `end` - Optional end index, can be negative to count from end (default: -1)
  - `BYTE | BIT` - Optional unit specification (default: BYTE)
  - Returns: Count of elements in the specified range
  - Behaves similarly to Redis BITCOUNT command
- **`BITS.POS key bit [start [end [BYTE | BIT]]]`** - Find the position of the first bit set to 1 or 0
  - `key` - The bitset key
  - `bit` - Must be 0 or 1 (the bit value to search for)
  - `start` - Optional starting position (default: 0)
  - `end` - Optional ending position (default: end of bitset)
  - `BYTE | BIT` - Optional unit specification (default: BYTE)
  - Returns: Position of the first bit with the specified value, or -1 if not found
  - Behaves similarly to Redis BITPOS command

### Set Operations

- **`BITS.OR dest src1 [src2 ...]`** - Store union (src1 | src2 | ...) of bitsets in dest
  - Returns: Range of the resulting set in bytes
- **`BITS.AND dest src1 [src2 ...]`** - Store intersection (src1 & src2 & ...) of bitsets in dest
  - Returns: Range of the resulting set in bytes
- **`BITS.XOR dest src1 [src2 ...]`** - Store symmetric difference (src1 ^ src2 ^ ...) in dest
  - Returns: Range of the resulting set in bytes

### Utility Operations

- **`BITS.TOARRAY key`** - Get all elements as an array, sorted in ascending order
  - Returns: Array of all elements
- **`BITS.INFO key`** - Get detailed information about the bitset
  - Returns: Array with size, universe_size, allocated_memory, total_clusters, max_depth, hash_table

## Usage Examples

```bash
# Add elements to a bitset (bitset is created automatically)
redis-cli BITS.INSERT myset 1 5 10 100 1000
# Returns: (integer) 5

# Check if bits are set
redis-cli BITS.GET myset 5
# Returns: (integer) 1

redis-cli BITS.GET myset 7
# Returns: (integer) 0

# Count all elements
redis-cli BITS.COUNT myset
# Returns: (integer) 5

# Count elements in byte range 0-10
redis-cli BITS.COUNT myset 0 10
# Returns: (integer) 3

# Count elements in bit range 0-100
redis-cli BITS.COUNT myset 0 100 BIT
# Returns: (integer) 4

# Count elements from byte 5 to end
redis-cli BITS.COUNT myset 5 -1
# Returns: (integer) 2

# Get min and max
redis-cli BITS.MIN myset
# Returns: (integer) 1

redis-cli BITS.MAX myset
# Returns: (integer) 1000

# Find successor and predecessor
redis-cli BITS.SUCCESSOR myset 3
# Returns: (integer) 5

redis-cli BITS.PREDECESSOR myset 100
# Returns: (integer) 10

# List all elements
redis-cli BITS.TOARRAY myset
# Returns: 1) (integer) 1
#          2) (integer) 5
#          3) (integer) 10
#          4) (integer) 100
#          5) (integer) 1000

# Set operations
redis-cli BITS.INSERT set1 1 2 3 4
redis-cli BITS.INSERT set2 3 4 5 6

redis-cli BITS.OR result set1 set2
# Returns: (integer) 1  (elements: 1,2,3,4,5,6)

redis-cli BITS.AND result set1 set2
# Returns: (integer) 1  (elements: 3,4)

redis-cli BITS.XOR result set1 set2
# Returns: (integer) 1  (elements: 1,2,5,6)

# Remove elements
redis-cli BITS.REMOVE myset 5 10
# Returns: (integer) 2

# Get information
redis-cli BITS.INFO myset
# Returns detailed information about the bitset

# Clear all elements
redis-cli BITS.CLEAR myset
# Returns: OK

# Find bit positions
redis-cli BITS.INSERT postest 1 5 10 100
redis-cli BITS.POS postest 1
# Returns: (integer) 1  (first set bit)

redis-cli BITS.POS postest 0
# Returns: (integer) 0  (first unset bit)

redis-cli BITS.POS postest 1 0 1
# Returns: (integer) 1  (first set bit in byte range 0-1)

redis-cli BITS.POS postest 1 0 10 BIT
# Returns: (integer) 1  (first set bit in bit range 0-10)

redis-cli BITS.POS postest 0 2 8 BIT
# Returns: (integer) 2  (first unset bit in bit range 2-8)
```



## Performance Characteristics

- **Insert**: O(log log U) amortized, where U is the universe size
- **Delete/Get**: O(log log U)
- **Min/Max**: O(1)
- **Successor/Predecessor**: O(log log U)
- **Memory usage**: O(n), where n is the number of set bits
- **Set operations**: O(n + m), where n and m are the sizes of the input sets

## Implementation Details

The module uses van Emde Boas trees as the underlying data structure, which provides excellent performance for sparse bitsets. The VEB tree recursively divides the universe into clusters, allowing for very fast operations even with large universe sizes.

The module supports multiple hash table implementations:
- **std**: Uses `std::unordered_map` (always available)
- **absl**: Uses `absl::flat_hash_map` (if Abseil is available)
- **boost**: Uses `boost::unordered_flat_map` (if Boost is available)

## License

This module is part of the VEB Tree implementation project.
