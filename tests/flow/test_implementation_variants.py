from RLTest import Env


def test_hash_table_name(env: Env):
    env.cmd("BITS.INSERT", "impl", 1)
    info = env.cmd("BITS.INFO", "impl")
    info_map = dict(zip(info[::2], info[1::2]))
    hash_name = info_map[b'hash_table']
    env.assertIn(b'unordered', hash_name) 