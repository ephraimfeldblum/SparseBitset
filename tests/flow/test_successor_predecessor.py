from RLTest import Env


def test_successor_predecessor(env: Env):
    env.cmd("BITS.INSERT", "sp", 1, 5, 9)

    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 0), 1)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 5), 9)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 9), None)

    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp", 10), 9)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp", 1), None)


def test_sp_non_existent(env: Env):
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "nonexistent", 10), None)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "nonexistent", 10), None)


def test_sp_empty(env: Env):
    env.cmd("BITS.CLEAR", "sp")
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 10), None)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp", 10), None)


def test_sp_boundaries(env: Env):
    env.cmd("BITS.INSERT", "spb", 0, 100)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "spb", 0), 100)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "spb", 100), 0)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "spb", 0), None)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "spb", 100), None)
