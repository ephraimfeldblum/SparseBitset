from RLTest import Env


def test_rdb_persistence(env: Env):
    env.cmd("bits.insert", "persist", 42)
    env.dumpAndReload()  # Save RDB and restart
    env.assertEqual(env.cmd("bits.contains", "persist", 42), 1) 