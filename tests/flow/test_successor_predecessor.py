from RLTest import Env


def test_successor_predecessor(env: Env):
    env.cmd("BITS.INSERT", "sp", 1, 5, 9)

    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 0), 1)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 5), 9)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "sp", 9), None)

    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp", 10), 9)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", "sp", 1), None) 