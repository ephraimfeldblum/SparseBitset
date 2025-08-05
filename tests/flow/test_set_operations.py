from RLTest import Env


def test_set_operations(env: Env):
    env.cmd("bits.insert", "s1", 1, 2, 3, 4)
    env.cmd("bits.insert", "s2", 3, 4, 5, 6)

    # union into dest
    env.assertEqual(env.cmd("bits.or", "u", "s1", "s2"), 6)
    env.assertEqual(env.cmd("bits.toarray", "u"), [1, 2, 3, 4, 5, 6])

    # intersection
    env.assertEqual(env.cmd("bits.and", "i", "s1", "s2"), 2)
    env.assertEqual(env.cmd("bits.toarray", "i"), [3, 4])

    # diff (xor)
    env.assertEqual(env.cmd("bits.xor", "d", "s1", "s2"), 4)
    env.assertEqual(env.cmd("bits.toarray", "d"), [1, 2, 5, 6]) 