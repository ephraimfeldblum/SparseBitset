from RLTest import Env


def test_successor_predecessor(env: Env):
    env.cmd("bits.insert", "sp", 1, 5, 9)

    env.assertEqual(env.cmd("bits.successor", "sp", 0), 1)
    env.assertEqual(env.cmd("bits.successor", "sp", 5), 9)
    env.assertEqual(env.cmd("bits.successor", "sp", 9), None)

    env.assertEqual(env.cmd("bits.predecessor", "sp", 10), 9)
    env.assertEqual(env.cmd("bits.predecessor", "sp", 1), None) 