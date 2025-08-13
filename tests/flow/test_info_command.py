from RLTest import Env


def test_info_command(env: Env):
    env.cmd("BITS.INSERT", "info", 1, 100)
    info = env.cmd("BITS.INFO", "info")
    # Expected array length 6
    env.assertEqual(len(info), 10)
    # Size index 0 matches
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'size'], 2)
    env.assertGreaterEqual(info_map[b'universe_size'], 100)
