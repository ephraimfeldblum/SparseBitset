from RLTest import Env


def test_info_command(env: Env):
    env.cmd("BITS.INSERT", "info", 1, 100)
    info = env.cmd("BITS.INFO", "info")
    # Expected array length 10 (5 pairs)
    env.assertEqual(len(info), 10)
    # Convert alternating key/value list to a map
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'size'], 2)
    env.assertGreaterEqual(info_map[b'universe_size'], 100)
    # allocated_memory should be non-negative and total_clusters/max_depth present
    env.assertGreaterEqual(info_map[b'allocated_memory'], 0)
    env.assertIn(b'total_clusters', info_map)
    env.assertIn(b'max_depth', info_map)

    # Non-existent key should return an error
    env.expect("BITS.INFO", "no_such_key").error().contains("key does not exist or is not a bitset")

    # BITS.INFO on a non-bitset type should return WRONGTYPE
    env.cmd("SET", "notabits", "hello")
    env.expect("BITS.INFO", "notabits").error().contains("WRONGTYPE")
