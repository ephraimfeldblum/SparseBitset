from RLTest import Env


def test_set_operations(env: Env):
    env.cmd("BITS.INSERT", "s1", 1, 2, 3, 4)
    env.cmd("BITS.INSERT", "s2", 3, 4, 5, 6)

    # union into dest
    env.assertEqual(env.cmd("BITS.OR", "u", "s1", "s2"), 6)
    env.assertEqual(env.cmd("BITS.TOARRAY", "u"), [1, 2, 3, 4, 5, 6])

    # intersection
    env.assertEqual(env.cmd("BITS.AND", "i", "s1", "s2"), 2)
    env.assertEqual(env.cmd("BITS.TOARRAY", "i"), [3, 4])

    # diff (xor)
    env.assertEqual(env.cmd("BITS.XOR", "d", "s1", "s2"), 4)
    env.assertEqual(env.cmd("BITS.TOARRAY", "d"), [1, 2, 5, 6]) 