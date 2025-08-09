#include "redismodule.h"
#include "VEB/VebTree.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define BITSET_TYPE_NAME "sparsebit"

void* malloc(size_t size) {
    return RedisModule_Alloc(size);
}

void* realloc(void* ptr, size_t size) {
    return RedisModule_Realloc(ptr, size);
}

void free(void* ptr) {
    RedisModule_Free(ptr);
}

static RedisModuleType *BitsetType;

typedef struct {
    VebTree_Handle_t handle;
    const VebTree_API_t *api;
} Bitset;

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

static void bitset_free(Bitset *bitset) {
    if (bitset) {
        if (bitset->handle) {
            bitset->api->destroy(bitset->handle);
        }
        RedisModule_Free(bitset);
    }
}

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

    VebTree_ImplType_t impl_type = vebtree_get_impl_type(bitset->handle);
    RedisModule_SaveUnsigned(rdb, impl_type);

    size_t size = bitset->api->size(bitset->handle);
    RedisModule_SaveUnsigned(rdb, size);

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
        return RedisModule_ReplyWithError(ctx, REDISMODULE_ERRORMSG_WRONGTYPE);
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
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    bool bit_set = bitset->api->contains(bitset->handle, (size_t)offset);
    return RedisModule_ReplyWithLongLong(ctx, bit_set ? 1 : 0);
}

// bits.SET key offset value
static int bits_set_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc != 4) {
        return RedisModule_WrongArity(ctx);
    }

    long long offset;
    if (RedisModule_StringToLongLong(argv[2], &offset) != REDISMODULE_OK || offset < 0) {
        return RedisModule_ReplyWithError(ctx, "ERR bit offset is not an integer or out of range");
    }

    long long value;
    if (RedisModule_StringToLongLong(argv[3], &value) != REDISMODULE_OK || (value != 0 && value != 1)) {
        return RedisModule_ReplyWithError(ctx, "ERR bit value must be 0 or 1");
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_WRITE);
    if (!bitset) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to create or access bitset");
    }

    bool previous_bit_set = bitset->api->contains(bitset->handle, (size_t)offset);
    long long previous_value = previous_bit_set ? 1 : 0;

    if (value == 1) {
        bitset->api->insert(bitset->handle, (size_t)offset);
    } else {
        bitset->api->remove(bitset->handle, (size_t)offset);
    }

    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, previous_value);
}

static long long count_elements_in_range(Bitset *bitset, long long start, long long end, bool is_bit_range) {
    if (!bitset || start > end) {
        return 0;
    }

    long long bit_start = is_bit_range ? start : start * 8;
    long long bit_end = is_bit_range ? end : (end * 8) + 7;

    if (bit_start < 0) bit_start = 0;
    if (bit_end < 0) return 0;

    long long count = 0;

    VebTree_OptionalSize_t current;
    if (bit_start == 0) {
        current = bitset->api->min(bitset->handle);
    } else {
        current = bitset->api->successor(bitset->handle, (size_t)(bit_start - 1));
    }

    while (current.has_value && (long long)current.value <= bit_end) {
        count++;
        current = bitset->api->successor(bitset->handle, current.value);
    }

    return count;
}

// bits.COUNT key [start end [BYTE | BIT]]
static int bits_count_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 2 || argc > 5) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);
    if (!bitset) {
        return RedisModule_ReplyWithLongLong(ctx, 0);
    }

    if (argc == 2) {
        size_t size = bitset->api->size(bitset->handle);
        return RedisModule_ReplyWithLongLong(ctx, (long long)size);
    }

    long long start, end;
    if (RedisModule_StringToLongLong(argv[2], &start) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid start index");
    }
    if (RedisModule_StringToLongLong(argv[3], &end) != REDISMODULE_OK) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid end index");
    }

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

    if (end < 0) {
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

// bits.OP <AND | OR | XOR> destkey key [key ...]
static int bits_op_command(RedisModuleCtx *ctx, RedisModuleString **argv, int argc) {
    if (argc < 4) {
        return RedisModule_WrongArity(ctx);
    }

    // Parse the operation type
    const char *operation = RedisModule_StringPtrLen(argv[1], NULL);
    if (!operation) {
        return RedisModule_ReplyWithError(ctx, "ERR invalid operation");
    }

    // Validate operation type
    int op_type = -1;
    if (strcasecmp(operation, "AND") == 0) {
        op_type = 0;
    } else if (strcasecmp(operation, "OR") == 0) {
        op_type = 1;
    } else if (strcasecmp(operation, "XOR") == 0) {
        op_type = 2;
    } else {
        return RedisModule_ReplyWithError(ctx, "ERR syntax error, expected AND, OR, or XOR");
    }

    // Get destination key
    Bitset *dest = get_bitset_key(ctx, argv[2], REDISMODULE_WRITE);
    if (!dest) {
        return RedisModule_ReplyWithError(ctx, "ERR failed to create or access destination bitset");
    }

    // Clear destination
    dest->api->clear(dest->handle);

    if (op_type == 1) { // OR operation
        // For OR, union all source bitsets
        for (int i = 3; i < argc; i++) {
            Bitset *src = get_bitset_key(ctx, argv[i], REDISMODULE_READ);
            if (src) {
                dest->api->union_op(dest->handle, src->handle);
            }
            // Non-existent keys are treated as empty (all zeros), so we skip them
        }
    } else if (op_type == 0) { // AND operation
        // For AND, start with first source and intersect with others
        Bitset *first_src = get_bitset_key(ctx, argv[3], REDISMODULE_READ);
        if (!first_src) {
            // If first source doesn't exist, result is empty
            RedisModule_ReplicateVerbatim(ctx);
            return RedisModule_ReplyWithLongLong(ctx, 0);
        }

        // Copy first source to destination
        dest->api->union_op(dest->handle, first_src->handle);

        // Intersect with remaining sources
        for (int i = 4; i < argc; i++) {
            Bitset *src = get_bitset_key(ctx, argv[i], REDISMODULE_READ);
            if (src) {
                dest->api->intersection(dest->handle, src->handle);
            } else {
                // Non-existent key means all zeros, so result becomes empty
                dest->api->clear(dest->handle);
                break;
            }
        }
    } else if (op_type == 2) { // XOR operation
        // For XOR, start with first source and XOR with others
        Bitset *first_src = get_bitset_key(ctx, argv[3], REDISMODULE_READ);
        if (first_src) {
            dest->api->union_op(dest->handle, first_src->handle);
        }
        // Non-existent first source is treated as empty (all zeros)

        // XOR with remaining sources
        for (int i = 4; i < argc; i++) {
            Bitset *src = get_bitset_key(ctx, argv[i], REDISMODULE_READ);
            if (src) {
                dest->api->symmetric_difference(dest->handle, src->handle);
            }
            // Non-existent keys are treated as empty (all zeros), so XOR with them has no effect
        }
    }

    // Calculate result size in bytes (like Redis BITOP)
    size_t result_bytes = 0;
    VebTree_OptionalSize_t max_elem = dest->api->max(dest->handle);
    if (max_elem.has_value) {
        result_bytes = (max_elem.value / 8) + 1;
    }

    RedisModule_ReplicateVerbatim(ctx);
    return RedisModule_ReplyWithLongLong(ctx, (long long)result_bytes);
}

static long long find_bit_position(Bitset *bitset, int bit_value, long long start, long long end, bool is_bit_range) {
    if (!bitset) {
        return -1;
    }

    long long bit_start = is_bit_range ? start : start * 8;
    long long bit_end = is_bit_range ? end : (end * 8) + 7;

    if (bit_start < 0) bit_start = 0;
    if (bit_end < 0) return -1;

    if (bit_value == 1) {
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
    if (argc < 3 || argc > 6) {
        return RedisModule_WrongArity(ctx);
    }

    Bitset *bitset = get_bitset_key(ctx, argv[1], REDISMODULE_READ);

    long long bit_value;
    if (RedisModule_StringToLongLong(argv[2], &bit_value) != REDISMODULE_OK ||
        (bit_value != 0 && bit_value != 1)) {
        return RedisModule_ReplyWithError(ctx, "ERR bit value must be 0 or 1");
    }

    long long start = 0;
    long long end = -1;
    bool is_bit_range = false;

    if (argc >= 4) {
        if (RedisModule_StringToLongLong(argv[3], &start) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "ERR invalid start index");
        }
    }

    if (argc >= 5) {
        if (RedisModule_StringToLongLong(argv[4], &end) != REDISMODULE_OK) {
            return RedisModule_ReplyWithError(ctx, "ERR invalid end index");
        }
    }

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

    if (!bitset) {
        if (bit_value == 0) {
            return RedisModule_ReplyWithLongLong(ctx, start >= 0 ? start : 0);
        } else {
            return RedisModule_ReplyWithLongLong(ctx, -1);
        }
    }

    if (end < 0 && argc >= 5) {
        VebTree_OptionalSize_t max_elem = bitset->api->max(bitset->handle);
        if (max_elem.has_value) {
            long long max_index = is_bit_range ? (long long)max_elem.value : ((long long)max_elem.value / 8);
            end = max_index + end + 1;
            if (end < 0) {
                return RedisModule_ReplyWithLongLong(ctx, -1);
            }
        } else {
            if (bit_value == 0) {
                return RedisModule_ReplyWithLongLong(ctx, start >= 0 ? start : 0);
            } else {
                return RedisModule_ReplyWithLongLong(ctx, -1);
            }
        }
    } else if (end < 0) {
        VebTree_OptionalSize_t max_elem = bitset->api->max(bitset->handle);
        if (max_elem.has_value) {
            end = is_bit_range ? (long long)max_elem.value : ((long long)max_elem.value / 8);
        } else {
            if (bit_value == 0) {
                return RedisModule_ReplyWithLongLong(ctx, start >= 0 ? start : 0);
            } else {
                return RedisModule_ReplyWithLongLong(ctx, -1);
            }
        }
    }

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

    if (RedisModule_CreateCommand(ctx, "bits.set", bits_set_command, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
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

    if (RedisModule_CreateCommand(ctx, "bits.op", bits_op_command, "write deny-oom", 1, 1, 1) == REDISMODULE_ERR) {
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
