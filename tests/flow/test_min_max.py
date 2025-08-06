from RLTest import Env


def test_min_max(env: Env):
    env.cmd("BITS.INSERT", "mm", 10, 5, 30)
    env.assertEqual(env.cmd("BITS.MIN", "mm"), 5)
    env.assertEqual(env.cmd("BITS.MAX", "mm"), 30)

    # Remove current min and max
    env.cmd("BITS.REMOVE", "mm", 5, 30)
    env.assertEqual(env.cmd("BITS.MIN", "mm"), 10)
    env.assertEqual(env.cmd("BITS.MAX", "mm"), 10)

    # Empty set returns null
    env.cmd("BITS.CLEAR", "mm")
    env.assertEqual(env.cmd("BITS.MIN", "mm"), None)
    env.assertEqual(env.cmd("BITS.MAX", "mm"), None) 