from RLTest import Env

def test_large_values(env: Env):
    # 2^31 - 1
    val31 = 2147483647
    # 2^31
    val31p = 2147483648
    # 2^32 - 1
    val32 = 4294967295
    # 2^32
    val32p = 4294967296
    # 2^63 - 1 (Max signed long long)
    val63 = 9223372036854775807
    
    key = "large_set"
    env.assertEqual(env.cmd("BITS.INSERT", key, val31, val31p, val32, val32p, val63), 5)
    env.assertEqual(env.cmd("BITS.COUNT", key), 5)
    
    env.assertEqual(env.cmd("BITS.GET", key, val31), 1)
    env.assertEqual(env.cmd("BITS.GET", key, val31p), 1)
    env.assertEqual(env.cmd("BITS.GET", key, val32), 1)
    env.assertEqual(env.cmd("BITS.GET", key, val32p), 1)
    env.assertEqual(env.cmd("BITS.GET", key, val63), 1)
    
    # Successor/Predecessor with large values
    env.assertEqual(env.cmd("BITS.SUCCESSOR", key, val31), val31p)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", key, val32), val32p)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", key, val32p), val63)
    
    env.assertEqual(env.cmd("BITS.PREDECESSOR", key, val63), val32p)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", key, val32p), val32)
    
    # Range operations (implicitly tested by BITS.TOARRAY if it works)
    # Note: BITS.TOARRAY might be slow if we have many bits, but here we only have 5.
    env.assertEqual(env.cmd("BITS.TOARRAY", key), [val31, val31p, val32, val32p, val63])

def test_max_boundary(env: Env):
    key = "boundary"
    max_ll = 9223372036854775807
    
    env.assertEqual(env.cmd("BITS.INSERT", key, max_ll), 1)
    env.assertEqual(env.cmd("BITS.GET", key, max_ll), 1)
    env.assertEqual(env.cmd("BITS.MAX", key), max_ll)
    
    # Testing overflow - RedisModule_StringToLongLong should fail or wrap for values > LLONG_MAX
    # depending on implementation. Usually "ERR invalid element value" because of our check.
    env.expect("BITS.INSERT", key, "9223372036854775808").error().contains("invalid element")
