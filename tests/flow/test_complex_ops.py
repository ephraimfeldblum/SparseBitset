from RLTest import Env

def test_op_dest_is_source(env: Env):
    """Test BITS.OP where destination is also one of the source keys"""
    env.cmd("BITS.INSERT", "s1", 1, 2, 3)
    env.cmd("BITS.INSERT", "s2", 3, 4, 5)
    
    # OR dest=s1
    env.assertEqual(env.cmd("BITS.OP", "OR", "s1", "s1", "s2"), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "s1"), 5)
    env.assertEqual(env.cmd("BITS.TOARRAY", "s1"), [1, 2, 3, 4, 5])
    
    # AND dest=s2
    env.cmd("BITS.INSERT", "s3", 4, 5, 6)
    env.assertEqual(env.cmd("BITS.OP", "AND", "s2", "s2", "s3"), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "s2"), 2)
    env.assertEqual(env.cmd("BITS.TOARRAY", "s2"), [4, 5])

def test_many_sources(env: Env):
    """Test BITS.OP with a large number of source keys"""
    num_sources = 100
    for i in range(num_sources):
        env.cmd("BITS.INSERT", f"src{i}", i)
    
    source_keys = [f"src{i}" for i in range(num_sources)]
    env.assertEqual(env.cmd("BITS.OP", "OR", "total", *source_keys), num_sources//8 + 1)
    env.assertEqual(env.cmd("BITS.COUNT", "total"), num_sources)
    
    env.assertEqual(env.cmd("BITS.OP", "AND", "empty_and", *source_keys), 0)
    env.assertEqual(env.cmd("BITS.COUNT", "empty_and"), 0)

def test_large_insert_arity(env: Env):
    """Test BITS.INSERT with many arguments"""
    vals = list(range(1000))
    env.assertEqual(env.cmd("BITS.INSERT", "big", *vals), 1000)
    env.assertEqual(env.cmd("BITS.COUNT", "big"), 1000)

def test_delete_and_recreate(env: Env):
    """Test deleting a bitset key and using it again"""
    env.cmd("BITS.INSERT", "delkey", 1, 2, 3)
    env.cmd("DEL", "delkey")
    
    # Should not be WRONGTYPE, should be treated as empty
    env.assertEqual(env.cmd("BITS.COUNT", "delkey"), 0)
    env.assertEqual(env.cmd("BITS.GET", "delkey", 1), 0)
    
    # Re-insert
    env.assertEqual(env.cmd("BITS.INSERT", "delkey", 10), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "delkey"), 1)

def test_mixed_op_with_empty(env: Env):
    """Test BITS.OP with mixture of existing and non-existing keys"""
    env.cmd("BITS.INSERT", "e1", 1)
    # e2 is non-existent
    env.cmd("BITS.INSERT", "e3", 3)
    
    env.assertEqual(env.cmd("BITS.OP", "OR", "res", "e1", "e2", "e3"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "res"), [1, 3])
    
    env.assertEqual(env.cmd("BITS.OP", "AND", "res_and", "e1", "e2", "e3"), 0)
    env.assertEqual(env.cmd("BITS.COUNT", "res_and"), 0)


def test_or_op(env: Env):
    key_prefix = "dl_src_"
    sizes = [10, 18, 26, 34, 42, 50, 58, 66, 74, 82, 90, 98, 106, 114, 122]
    ref_sets = {}

    for i, sz in enumerate(sizes):
        base = i * 37
        vals = [base + j for j in range(sz)]
        key = f"{key_prefix}{i}"
        ref_sets[key] = set(vals)
        env.cmd("BITS.INSERT", key, *vals)

    source_keys = [f"{key_prefix}{i}" for i in range(len(sizes))]
    dest = "dl_dest_or"

    ret_bytes = env.cmd("BITS.OP", "OR", dest, *source_keys)

    # Compute expected
    expected = set()
    for k in source_keys:
        expected |= ref_sets[k]

    if not expected:
        env.assertEqual(ret_bytes, 0)
    else:
        env.assertEqual(ret_bytes, (max(expected) // 8) + 1)

    env.assertEqual(env.cmd("BITS.COUNT", dest), len(expected))
    # TOARRAY check for reasonably sized result
    env.assertEqual(sorted(list(expected)), env.cmd("BITS.TOARRAY", dest))


def test_or_op_dest_is_source(env: Env):
    key_prefix = "dds_src_"
    sizes = [12, 20, 16, 28, 24, 32, 16, 8, 40, 36, 44, 52, 6]
    ref_sets = {}

    for i, sz in enumerate(sizes):
        base = 1000 + i * 13
        vals = [base + j * 137 for j in range(sz)]
        key = f"{key_prefix}{i}"
        ref_sets[key] = set(vals)
        env.cmd("BITS.INSERT", key, *vals)

    # Use the first source as destination as well
    source_keys = [f"{key_prefix}{i}" for i in range(len(sizes))]
    dest = source_keys[0]

    ret_bytes = env.cmd("BITS.OP", "OR", dest, *source_keys)

    expected = ref_sets[source_keys[0]].copy()
    for k in source_keys[1:]:
        expected |= ref_sets[k]

    if not expected:
        env.assertEqual(ret_bytes, 0)
    else:
        env.assertEqual(ret_bytes, (max(expected) // 8) + 1)

    env.assertEqual(env.cmd("BITS.COUNT", dest), len(expected))
    env.assertEqual(sorted(list(expected)), env.cmd("BITS.TOARRAY", dest))
