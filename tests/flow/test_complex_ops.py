from RLTest import Env

def test_op_dest_is_source(env: Env):
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
    num_sources = 100
    for i in range(num_sources):
        env.cmd("BITS.INSERT", f"src{i}", i)
    
    source_keys = [f"src{i}" for i in range(num_sources)]
    env.assertEqual(env.cmd("BITS.OP", "OR", "total", *source_keys), num_sources//8 + 1)
    env.assertEqual(env.cmd("BITS.COUNT", "total"), num_sources)
    
    env.assertEqual(env.cmd("BITS.OP", "AND", "empty_and", *source_keys), 0)
    env.assertEqual(env.cmd("BITS.COUNT", "empty_and"), 0)

def test_large_insert_arity(env: Env):
    vals = list(range(1000))
    env.assertEqual(env.cmd("BITS.INSERT", "big", *vals), 1000)
    env.assertEqual(env.cmd("BITS.COUNT", "big"), 1000)

def test_delete_and_recreate(env: Env):
    env.cmd("BITS.INSERT", "delkey", 1, 2, 3)
    env.cmd("DEL", "delkey")
    
    # Should not be WRONGTYPE, should be treated as empty
    env.assertEqual(env.cmd("BITS.COUNT", "delkey"), 0)
    env.assertEqual(env.cmd("BITS.GET", "delkey", 1), 0)
    
    # Re-insert
    env.assertEqual(env.cmd("BITS.INSERT", "delkey", 10), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "delkey"), 1)

def test_mixed_op_with_empty(env: Env):
    env.cmd("BITS.INSERT", "e1", 1)
    # e2 is non-existent
    env.cmd("BITS.INSERT", "e3", 3)
    
    env.assertEqual(env.cmd("BITS.OP", "OR", "res", "e1", "e2", "e3"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "res"), [1, 3])
    
    env.assertEqual(env.cmd("BITS.OP", "AND", "res_and", "e1", "e2", "e3"), 0)
    env.assertEqual(env.cmd("BITS.COUNT", "res_and"), 0)


def test_pos_count_and_set_edgecases(env: Env):
    # POS on an existing set
    env.cmd("BITS.INSERT", "p", 2, 5, 10)
    env.assertEqual(env.cmd("BITS.POS", "p", 1), 2)
    env.assertEqual(env.cmd("BITS.POS", "p", 1, 3, "-1", "BIT"), 5)
    env.assertEqual(env.cmd("BITS.POS", "p", 0), 0)

    # POS on non-existent key
    env.assertEqual(env.cmd("BITS.POS", "nokey", 0), 0)
    env.assertEqual(env.cmd("BITS.POS", "nokey", 1), -1)

    # Byte vs bit counting and negative end handling
    env.cmd("BITS.INSERT", "r", 0, 7, 8, 15)
    # BYTE 0..1 covers bits 0..15 -> 4 set bits
    env.assertEqual(env.cmd("BITS.COUNT", "r", "0", "1", "BYTE"), 4)
    # BIT 0..7 covers bits 0..7 -> bits at 0 and 7
    env.assertEqual(env.cmd("BITS.COUNT", "r", "0", "7", "BIT"), 2)
    # negative end -1 should include up to last element (same as full count here)
    env.assertEqual(env.cmd("BITS.COUNT", "r", "0", "-1", "BIT"), 4)

    # BITS.SET/BITS.GET invalid args
    env.expect("BITS.SET", "r", "-1", "1").error().contains("bit offset is not an integer or out of range")
    env.expect("BITS.SET", "r", "1", "2").error().contains("bit value must be 0 or 1")
    env.expect("BITS.GET", "r", "-5").error().contains("bit offset is not an integer or out of range")


def test_pos_boundaries_and_set_behavior(env: Env):
    env.cmd("BITS.CLEAR", "pb")
    # empty key: POS for 1 => -1, for 0 => start (0)
    env.assertEqual(env.cmd("BITS.POS", "pb", 1), -1)
    env.assertEqual(env.cmd("BITS.POS", "pb", 0), 0)

    # Insert some bits and test negative start handling (should be treated relative to max)
    env.cmd("BITS.INSERT", "pb", 5, 20)
    # start= -1 (bit) should resolve to last bit; provide both start and end and unit
    env.assertEqual(env.cmd("BITS.POS", "pb", 1, "-1", "-1", "BIT"), 20)

    # BITS.SET returns previous value; test toggling
    env.assertEqual(env.cmd("BITS.GET", "pb", 10), 0)
    env.assertEqual(env.cmd("BITS.SET", "pb", 10, 1), 0)
    env.assertEqual(env.cmd("BITS.GET", "pb", 10), 1)
    env.assertEqual(env.cmd("BITS.SET", "pb", 10, 0), 1)
    env.assertEqual(env.cmd("BITS.GET", "pb", 10), 0)


def test_count_range_edges_and_units(env: Env):
    env.cmd("DEL", "cr")
    # set bits spanning multiple bytes: bits 0,7,8,15,31
    env.cmd("BITS.INSERT", "cr", 0, 7, 8, 15, 31)

    # count whole key
    env.assertEqual(env.cmd("BITS.COUNT", "cr"), 5)

    # BYTE ranges: byte 0 covers bits 0..7 -> bits 0 and 7 => 2
    env.assertEqual(env.cmd("BITS.COUNT", "cr", "0", "0", "BYTE"), 2)

    # BIT ranges: 0..15 includes bits 0,7,8,15 => 4
    env.assertEqual(env.cmd("BITS.COUNT", "cr", "0", "15", "BIT"), 4)

    # start beyond last should return 0
    env.assertEqual(env.cmd("BITS.COUNT", "cr", "100", "200", "BIT"), 0)


def test_op_xor_or_sizes(env: Env):
    env.cmd("DEL", "a", "b", "xora", "ora")
    env.cmd("BITS.INSERT", "a", 1, 3, 5)
    env.cmd("BITS.INSERT", "b", 3, 4, 10)

    # XOR: should contain 1,5,4,10, size in bytes should be >0
    xor_bytes = env.cmd("BITS.OP", "XOR", "xora", "a", "b")
    env.assertGreaterEqual(xor_bytes, 1)
    arr = env.cmd("BITS.TOARRAY", "xora")
    env.assertEqual(sorted(arr), [1,4,5,10])
    # OR: union should contain 1,3,4,5,10
    ora_bytes = env.cmd("BITS.OP", "OR", "ora", "a", "b")
    env.assertGreaterEqual(ora_bytes, 1)
    arr_or = env.cmd("BITS.TOARRAY", "ora")
    env.assertEqual(sorted(arr_or), [1,3,4,5,10])


def test_clear_remove_and_min_max_edges(env: Env):
    env.cmd("DEL", "cm")
    env.assertEqual(env.cmd("BITS.COUNT", "cm"), 0)

    # Insert, remove some, clear and check behavior
    env.cmd("BITS.INSERT", "cm", 2, 4, 8)
    env.assertEqual(env.cmd("BITS.REMOVE", "cm", 4), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "cm"), [2,8])

    # Clear
    env.cmd("BITS.CLEAR", "cm")
    env.assertEqual(env.cmd("BITS.COUNT", "cm"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "cm"), [])

    # MIN/MAX on empty should return None
    env.assertEqual(env.cmd("BITS.MIN", "cm"), None)
    env.assertEqual(env.cmd("BITS.MAX", "cm"), None)


def test_successor_predecessor_edges(env: Env):
    env.cmd("DEL", "sp2")
    # Non-existent key: successor/predecessor should return None
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp2", 0), None)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp2", 10), None)

    env.cmd("BITS.INSERT", "sp2", 5, 15)
    # successor beyond last -> None, predecessor below first -> None
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp2", 15), None)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp2", 5), None)
