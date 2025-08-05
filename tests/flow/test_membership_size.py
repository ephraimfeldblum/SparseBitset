from RLTest import Env


def test_membership_and_size(env: Env):
    env.assertEqual(env.cmd("BITS.INSERT", "ms", 1, 2, 3), 3)
    env.assertEqual(env.cmd("BITS.COUNT", "ms"), 3)
    env.assertEqual(env.cmd("BITS.GET", "ms", 2), 1)
    env.assertEqual(env.cmd("BITS.GET", "ms", 4), 0)

    # remove one element
    env.assertEqual(env.cmd("BITS.REMOVE", "ms", 2), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "ms"), 2)
    env.assertEqual(env.cmd("BITS.GET", "ms", 2), 0) 