#include "redismodule.h"
#include "VEB/VebTree.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>  // For strcasecmp

#define BITSET_TYPE_NAME "sparsebit"

// Redis data type for bitset
static RedisModuleType *BitsetType;

// Bitset structure that wraps VebTree
typedef struct {
    VebTree_Handle_t handle;
    const VebTree_API_t *api;
} Bitset;

// Create a new bitset
static Bitset *bitset_create(VebTree_ImplType_t impl_type) {
    Bitset *bitset = RedisModule_Alloc(sizeof(Bitset));
    bitset->handle = vebtree_create(impl_type);
    if (!bitset->handle) {
        RedisModule_Free(bitset);
        return NULL;
    }
    bitset->api = vebtree_get_api(bitset->handle);
    return bitset;
}

// Free a bitset
static void bitset_free(Bitset *bitset) {
    if (bitset) {
        if (bitset->handle) {
            bitset->api->destroy(bitset->handle);
        }
        RedisModule_Free(bitset);
    }
}

// Redis type methods
static void *bitset_rdb_load(RedisModuleIO *rdb, int encver) {
    if (encver != 0) {
        return NULL;
    }
    
    VebTree_ImplType_t impl_type = RedisModule_LoadUnsigned(rdb);
    size_t count = RedisModule_LoadUnsigned(rdb);
    
    Bitset *bitset = bitset_create(impl_type);
    if (!bitset) {
        return NULL;
    }
    
    for (size_t i = 0; i < count; i++) {
        size_t value = RedisModule_LoadUnsigned(rdb);
        bitset->api->insert(bitset->handle, value);
    }
    
    return bitset;
}

static void bitset_rdb_save(RedisModuleIO *rdb, void *value) {
    Bitset *bitset = value;

    // Save implementation type
    VebTree_ImplType_t impl_type = vebtree_get_impl_type(bitset->handle);
    RedisModule_SaveUnsigned(rdb, impl_type);

    // Save size
    size_t size = bitset->api->size(bitset->handle);
    RedisModule_SaveUnsigned(rdb, size);

    // Save all elements
    size_t *array = bitset->api->to_array(bitset->handle);
    for (size_t i = 0; i < size; i++) {
        RedisModule_SaveUnsigned(rdb, array[i]);
    }
    free(array);
}

static void bitset_aof_rewrite(RedisModuleIO *aof, RedisModuleString *key, void *value) {
    Bitset *bitset = value;
    size_t size = bitset->api->size(bitset->handle);

    if (size > 0) {
        size_t *array = bitset->api->to_array(bitset->handle);
        for (size_t i = 0; i < size; i++) {
            RedisModule_EmitAOF(aof, "bits.insert", "sl", key, array[i]);
        }
        free(array);
    }
}

static void bitset_free_wrapper(void *value) {
    bitset_free((Bitset*)value);
}

static size_t bitset_mem_usage(const void *value) {
    const Bitset *bitset = value;
    return bitset->api->get_allocated_memory(bitset->handle);
}

static int bitset_defrag(RedisModuleDefragCtx *ctx, RedisModuleString *key, void **value) {
    (void)ctx;
    (void)key;
    (void)value;
    return 0;
}

// Helper function to get or create bitset
static Bitset *get_bitset_key(RedisModuleCtx *ctx, RedisModuleString *keyname, int mode) {
    RedisModuleKey *key = RedisModule_OpenKey(ctx, keyname, mode);
    int type = RedisModule_KeyType(key);
    
    if (type == REDISMODULE_KEYTYPE_EMPTY) {
        if (mode == REDISMODULE_WRITE) {
            Bitset *bitset = bitset_create(VEBTREE_STD);
            if (!bitset) {
                RedisModule_CloseKey(key);
                return NULL;
            }
            RedisModule_ModuleTypeSetValue(key, BitsetType, bitset);
            RedisModule_CloseKey(key);
            return bitset;
        } else {
            RedisModule_CloseKey(key);
            return NULL;
        }
    } else if (RedisModule_ModuleTypeGetType(key) != BitsetType) {
        RedisModule_CloseKey(key);
        return NULL;
    }
    
    Bitset *bitset = RedisModule_ModuleTypeGetValue(key);
    RedisModule_CloseKey(key);
    return bitset;
}

// bits.INSERT key element [element ...]
static int bits_add_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }
    
    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!bitset) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to create or access bitset");
    }
    
    long long added = 0;
    for (int i = 2; i < argc; i++) {
        long long element;
        if (RedisModule_StringToLongLong(argv[i], &element) != REDISMODULE_OK || element < 0) {
            return RedisModule_ReplyWithError(ctx, "ERR invalid element value");
        }
        
        if (!bitset->api->contains(bitset->handle, (size_t)element)) {
            bitset->api->insert(bitset->handle, (size_t)element);
            added++;
        }
    }
    
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, added);
}

// bits.REMOVE key element [element ...]
static int bits_rem_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }
    
    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!bitset) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }
    
    long long removed = 0;
    for (int i = 2; i < argc; i++) {
        long long element;
        if (RedisModule_StringToLongLong(argv[i], &element) != REDISMODULE_OK || element < 0) {
            return RedisModule_ReplyWithError(ctx, "ERR invalid element value");
        }
        
        if (bitset->api->contains(bitset->handle, (size_t)element)) {
            bitset->api->remove(bitset->handle, (size_t)element);
            removed++;
        }
    }
    
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, removed);
}

// bits.GET key offset
static int bits_get_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    long long offset;
    if (RedisModule_StringToLongLong(argv[2], &offset) != REDISMODULE_OK || offset < 0) {
        return RedisModule_ReplyWithError(ctx, "ERR bit offset is not an integer or out of range");
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        // When key does not exist, it is assumed to be an empty string, so offset is always out of range and the value is 0
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    // Check if the bit at offset is set
    bool bit_set = bitset->api->contains(bitset->handle, (size_t)offset);
    return RedisModule_ReplyWithLongLong(ctx, bit_set ? 1 : 0);
}

// Helper function to count elements in a range
static long long count_elements_in_range(Bitset *bitset, long long start, long long end, bool is_bit_range) {
    if (!bitset || start > end) {
        return 0;
    }

    // Convert byte indices to bit indices if needed
    long long bit_start = is_bit_range ? start : start * 8;
    long long bit_end = is_bit_range ? end : (end * 8) + 7;

    // Ensure non-negative indices
    if (bit_start < 0) bit_start = 0;
    if (bit_end < 0) return 0;

    long long count = 0;

    // Find the first element >= bit_start
    VebTree_OptionalSize_t current;
    if (bit_start == 0) {
        current = bitset->api->min(bitset->handle);
    } else {
        current = bitset->api->successor(bitset->handle, (size_t)(bit_start - 1));
    }

    // Count elements in the range
    while (current.has_value && (long long)current.value <= bit_end) {
        count++;
        current = bitset->api->successor(bitset->handle, current.value);
    }

    return count;
}

// bits.COUNT key [start end [BYTE | BIT]]
static int bits_count_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // Validate argument count
    if (argc < 2 || argc > 5) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    // If no range specified, return total count
    if (argc == 2) {
        size_t size = bitset->api->size(bitset->handle);
        return RedisModule_ReplyWithLongLong(ctx, (long long)size);
    }

    // Parse start and end indices
    long long start, end;
    if (RedisModule_StringToLongLong(argv[2], &start) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid start index");
    }
    if (RedisModule_StringToLongLong(argv[3], &end) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid end index");
    }

    // Parse BYTE/BIT specification (default is BYTE)
    bool is_bit_range = false;
    if (argc == 5) {
        const char *unit = RedisModule_StringPtrLen(argv[4], NULL);
        if (strcasecmp(unit, "BIT") == 0) {
            is_bit_range = true;
        } else if (strcasecmp(unit, "BYTE") == 0) {
            is_bit_range = false;
        } else {
            return RedisModule_ReplyWithError(ctx, "ERR syntax error, expected BYTE or BIT");
        }
    }

    // Handle negative end index (count from end)
    if (end < 0) {
        // For negative end, we need to find the maximum element to calculate the actual end
        VebTree_OptionalSize_t max_elem = bitset->api->max(bitset->handle);
        if (!max_elem.has_value) {
            return RedisModule_ReplyWithLongLong(ctx, 0);
        }

        long long max_index = is_bit_range ? (long long)max_elem.value : ((long long)max_elem.value / 8);
        end = max_index + end + 1;
        if (end < 0) {
            return RedisModule_ReplyWithLongLong(ctx, 0);
        }
    }

    long long count = count_elements_in_range(bitset, start, end, is_bit_range);
    return RedisModule_ReplyWithLongLong(ctx, count);
}

// bits.CLEAR key
static int bits_clear_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    
    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!bitset) {
        return RedisModule_ReplyWithSimpleString(ctx, "OK");
    }
    
    bitset->api->clear(bitset->handle);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithSimpleString(ctx, "OK");
}

// bits.MIN key
static int bits_min_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }
    
    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithNull(ctx);
    }
    
    VebTree_OptionalSize_t result = bitset->api->min(bitset->handle);
    if (result.has_value) {
        return RedisModule_ReplyWithLongLong(ctx, (long long)result.value);
    } else {
        return RedisModule_ReplyWithNull(ctx);
    }
}

// bits.MAX key
static int bits_max_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithNull(ctx);
    }

    VebTree_OptionalSize_t result = bitset->api->max(bitset->handle);
    if (result.has_value) {
        return RedisModule_ReplyWithLongLong(ctx, (long long)result.value);
    } else {
        return RedisModule_ReplyWithNull(ctx);
    }
}

// bits.SUCCESSOR key element
static int bits_successor_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithNull(ctx);
    }

    long long element;
    if (RedisModule_StringToLongLong(argv[2], &element) != REDISMODULE_OK || element < 0) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid element value");
    }

    VebTree_OptionalSize_t result = bitset->api->successor(bitset->handle, (size_t)element);
    if (result.has_value) {
        return RedisModule_ReplyWithLongLong(ctx, (long long)result.value);
    } else {
        return RedisModule_ReplyWithNull(ctx);
    }
}

// bits.PREDECESSOR key element
static int bits_predecessor_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 3) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithNull(ctx);
    }

    long long element;
    if (RedisModule_StringToLongLong(argv[2], &element) != REDISMODULE_OK || element < 0) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid element value");
    }

    VebTree_OptionalSize_t result = bitset->api->predecessor(bitset->handle, (size_t)element);
    if (result.has_value) {
        return RedisModule_ReplyWithLongLong(ctx, (long long)result.value);
    } else {
        return RedisModule_ReplyWithNull(ctx);
    }
}

// bits.TOARRAY key
static int bits_toarray_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    size_t size = bitset->api->size(bitset->handle);
    if (size == 0) {
        return RedisModule_ReplyWithArray(ctx, 0);
    }

    size_t *array = bitset->api->to_array(bitset->handle);
    RedisModule_ReplyWithArray(ctx, size);
    for (size_t i = 0; i < size; i++) {
        RedisModule_ReplyWithLongLong(ctx, (long long)array[i]);
    }
    free(array);

    return REDISMODULE_OK;
}

// bits.OR dest src1 [src2 ...]
static int bits_or_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    // Get or create destination bitset
    Bitset *dest = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!dest) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to create or access destination bitset");
    }

    // Clear destination first
    dest->api->clear(dest->handle);

    // Union all source bitsets
    for (int i = 2; i < argc; i++) {
        Bitset *src = get_bitset_key(ctx, argv[i], REDISMODULE_READ);
        if (src) {
            dest->api->union_op(dest->handle, src->handle);
        }
    }

    size_t result_size = dest->api->size(dest->handle);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, (long long)result_size);
}

// bits.AND dest src1 [src2 ...]
static int bits_and_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    // Get first source bitset
    Bitset *first_src = get_bitset_key(ctx, argv[2], REDISMODULE_READ);
    if (!first_src) {
        // If first source doesn't exist, result is empty
        Bitset *dest = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
        if (dest) {
            dest->api->clear(dest->handle);
            RedisModule_ReplicateVerbatim(ctx);
        }
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    // Get or create destination and copy first source
    Bitset *dest = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!dest) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to create or access destination bitset");
    }

    // Copy first source to destination
    dest->api->clear(dest->handle);
    dest->api->union_op(dest->handle, first_src->handle);

    // Intersect with remaining sources
    for (int i = 3; i < argc; i++) {
        Bitset *src = get_bitset_key(ctx, argv[i], REDISMODULE_READ);
        if (src) {
            dest->api->intersection(dest->handle, src->handle);
        } else {
            // If any source doesn't exist, result is empty
            dest->api->clear(dest->handle);
            break;
        }
    }

    size_t result_size = dest->api->size(dest->handle);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, (long long)result_size);
}

// bits.XOR dest src1 [src2 ...]
static int bits_xor_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 3) {
        return RedisModule_WrongArity(ctx);
    }

    // Get first source bitset
    Bitset *first_src = get_bitset_key(ctx, argv[2], REDISMODULE_READ);
    if (!first_src) {
        // If first source doesn't exist, result is empty
        Bitset *dest = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
        if (dest) {
            dest->api->clear(dest->handle);
            RedisModule_ReplicateVerbatim(ctx);
        }
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    // Get or create destination and copy first source
    Bitset *dest = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!dest) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to create or access destination bitset");
    }

    // Copy first source to destination
    dest->api->clear(dest->handle);
    dest->api->union_op(dest->handle, first_src->handle);

    // XOR with remaining sources
    for (int i = 3; i < argc; i++) {
        Bitset *src = get_bitset_key(ctx, argv[i], REDISMODULE_READ);
        if (src) {
            dest->api->symmetric_difference(dest->handle, src->handle);
        }
    }

    size_t result_size = dest->api->size(dest->handle);
    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, (long long)result_size);
}

// Helper function to find the position of the first bit set to the specified value
static long long find_bit_position(Bitset *bitset, int bit_value, long long start, long long end, bool is_bit_range) {
    if (!bitset) {
        return -1;
    }

    // Convert byte indices to bit indices if needed
    long long bit_start = is_bit_range ? start : start * 8;
    long long bit_end = is_bit_range ? end : (end * 8) + 7;

    // Ensure non-negative indices
    if (bit_start < 0) bit_start = 0;
    if (bit_end < 0) return -1;

    if (bit_value == 1) {
        // Looking for first set bit (1)
        VebTree_OptionalSize_t current;
        if (bit_start == 0) {
            current = bitset->api->min(bitset->handle);
        } else {
            current = bitset->api->successor(bitset->handle, (size_t)(bit_start - 1));
        }

        if (current.has_value && (long long)current.value <= bit_end) {
            return (long long)current.value;
        }
    } else {
        // Looking for first unset bit (0)
        // We need to scan through the range and find the first position that's not set
        for (long long pos = bit_start; pos <= bit_end; pos++) {
            if (!bitset->api->contains(bitset->handle, (size_t)pos)) {
                return pos;
            }
        }
    }

    return -1;
}

// bits.POS key bit [start [end [BYTE | BIT]]]
static int bits_pos_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    // Validate argument count
    if (argc < 3 || argc > 6) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);

    // Parse bit value (0 or 1)
    long long bit_value;
    if (RedisModule_StringToLongLong(argv[2], &bit_value) != REDISMODULE_OK ||
        (bit_value != 0 && bit_value != 1)) {
        return RedisModule_ReplyWithError(ctx, "ERR bit value must be 0 or 1");
    }

    // Default range covers entire bitset
    long long start = 0;
    long long end = -1;  // Will be set to max position if not specified
    bool is_bit_range = false;

    // Parse start index if provided
    if (argc >= 4) {
        if (RedisModule_StringToLongLong(argv[3], &start) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "ERR invalid start index");
        }
    }

    // Parse end index if provided
    if (argc >= 5) {
        if (RedisModule_StringToLongLong(argv[4], &end) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "ERR invalid end index");
        }
    }

    // Parse BYTE/BIT specification (default is BYTE)
    if (argc == 6) {
        const char *unit = RedisModule_StringPtrLen(argv[5], NULL);
        if (strcasecmp(unit, "BIT") == 0) {
            is_bit_range = true;
        } else if (strcasecmp(unit, "BYTE") == 0) {
            is_bit_range = false;
        } else {
            return RedisModule_ReplyWithError(ctx, "ERR syntax error, expected BYTE or BIT");
        }
    }

    // Handle non-existent key (treated as empty string)
    if (!bitset) {
        if (bit_value == 0) {
            // First unset bit in empty string is at position 0
            return RedisModule_ReplyWithLongLong(ctx, start >= 0 ? start : 0);
        } else {
            // No set bits in empty string
            return RedisModule_ReplyWithLongLong(ctx, -1);
        }
    }

    // Handle negative end index (count from end)
    if (end < 0 && argc >= 5) {
        // For negative end, we need to find the maximum element to calculate the actual end
        VebTree_OptionalSize_t max_elem = bitset->api->max(bitset->handle);
        if (max_elem.has_value) {
            long long max_index = is_bit_range ? (long long)max_elem.value : ((long long)max_elem.value / 8);
            end = max_index + end + 1;
            if (end < 0) {
                return RedisModule_ReplyWithLongLong(ctx, -1);
            }
        } else {
            // Empty bitset, treat as if searching in empty string
            if (bit_value == 0) {
                return RedisModule_ReplyWithLongLong(ctx, start >= 0 ? start : 0);
            } else {
                return RedisModule_ReplyWithLongLong(ctx, -1);
            }
        }
    } else if (end < 0) {
        // If end is not specified, search to the end of the bitset
        VebTree_OptionalSize_t max_elem = bitset->api->max(bitset->handle);
        if (max_elem.has_value) {
            end = is_bit_range ? (long long)max_elem.value : ((long long)max_elem.value / 8);
        } else {
            // Empty bitset
            if (bit_value == 0) {
                return RedisModule_ReplyWithLongLong(ctx, start >= 0 ? start : 0);
            } else {
                return RedisModule_ReplyWithLongLong(ctx, -1);
            }
        }
    }

    // Handle negative start index
    if (start < 0) {
        VebTree_OptionalSize_t max_elem = bitset->api->max(bitset->handle);
        if (max_elem.has_value) {
            long long max_index = is_bit_range ? (long long)max_elem.value : ((long long)max_elem.value / 8);
            start = max_index + start + 1;
            if (start < 0) {
                start = 0;
            }
        } else {
            start = 0;
        }
    }

    long long position = find_bit_position(bitset, (int)bit_value, start, end, is_bit_range);
    return RedisModule_ReplyWithLongLong(ctx, position);
}

// bits.INFO key
static int bits_info_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 2) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithError(ctx, "ERR key does not exist or is not a bitset");
    }

    VebTree_MemoryStats_t stats = bitset->api->get_memory_stats(bitset->handle);
    size_t allocated_memory = bitset->api->get_allocated_memory(bitset->handle);
    size_t universe_size = bitset->api->universe_size(bitset->handle);
    size_t size = bitset->api->size(bitset->handle);
    const char *hash_table_name = bitset->api->hash_table_name();

    RedisModule_ReplyWithArray(ctx, 12);
    RedisModule_ReplyWithSimpleString(ctx, "size");
    RedisModule_ReplyWithLongLong(ctx, (long long)size);
    RedisModule_ReplyWithSimpleString(ctx, "universe_size");
    RedisModule_ReplyWithLongLong(ctx, (long long)universe_size);
    RedisModule_ReplyWithSimpleString(ctx, "allocated_memory");
    RedisModule_ReplyWithLongLong(ctx, (long long)allocated_memory);
    RedisModule_ReplyWithSimpleString(ctx, "total_clusters");
    RedisModule_ReplyWithLongLong(ctx, (long long)stats.total_clusters);
    RedisModule_ReplyWithSimpleString(ctx, "max_depth");
    RedisModule_ReplyWithLongLong(ctx, (long long)stats.max_depth);
    RedisModule_ReplyWithSimpleString(ctx, "hash_table");
    RedisModule_ReplyWithSimpleString(ctx, hash_table_name);

    return REDISMODULE_OK;
}

// Module initialization
int RedisModule_OnLoad(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    (void)argv;
    (void)argc;

    if (RedisModule_Init(ctx, "sparsebit", 1, REDISMODULE_APIVER_1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.insert", bits_add_command, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.remove", bits_rem_command, "write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.get", bits_get_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.count", bits_count_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.clear", bits_clear_command, "write", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.min", bits_min_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }
    
    if (RedisModule_CreateCommand(ctx, "bits.max", bits_max_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.successor", bits_successor_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.predecessor", bits_predecessor_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.toarray", bits_toarray_command, "readonly", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.or", bits_or_command, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.and", bits_and_command, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.xor", bits_xor_command, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.pos", bits_pos_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    if (RedisModule_CreateCommand(ctx, "bits.info", bits_info_command, "readonly fast", 1, 1, 1) == REDISMODULE_ERR) {
        return REDISMODULE_ERR;
    }

    RedisModuleTypeMethods tm = {
        .version = REDISMODULE_TYPE_METHOD_VERSION,
        .rdb_load = bitset_rdb_load,
        .rdb_save = bitset_rdb_save,
        .aof_rewrite = bitset_aof_rewrite,
        .free = bitset_free_wrapper,
        .mem_usage = bitset_mem_usage,
        .defrag = bitset_defrag,
    };
    
    BitsetType = RedisModule_CreateDataType(ctx, BITSET_TYPE_NAME, 0, &tm);
    if (BitsetType == NULL) {
        return REDISMODULE_ERR;
    }
    
    return REDISMODULE_OK;
}
