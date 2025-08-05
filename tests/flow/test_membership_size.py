from RLTest import Env


def test_membership_and_size(env: Env):
    env.assertEqual(env.cmd("bits.insert", "ms", 1, 2, 3), 3)
    env.assertEqual(env.cmd("bits.size", "ms"), 3)
    env.assertEqual(env.cmd("bits.contains", "ms", 2), 1)
    env.assertEqual(env.cmd("bits.contains", "ms", 4), 0)

    # remove one element
    env.assertEqual(env.cmd("bits.remove", "ms", 2), 1)
    env.assertEqual(env.cmd("bits.size", "ms"), 2)
    env.assertEqual(env.cmd("bits.contains", "ms", 2), 0) 