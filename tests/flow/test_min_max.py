from RLTest import Env


def test_min_max(env: Env):
    env.cmd("bits.insert", "mm", 10, 5, 30)
    env.assertEqual(env.cmd("bits.min", "mm"), 5)
    env.assertEqual(env.cmd("bits.max", "mm"), 30)

    # Remove current min and max
    env.cmd("bits.remove", "mm", 5, 30)
    env.assertEqual(env.cmd("bits.min", "mm"), 10)
    env.assertEqual(env.cmd("bits.max", "mm"), 10)

    # Empty set returns null
    env.cmd("bits.clear", "mm")
    env.assertEqual(env.cmd("bits.min", "mm"), None)
    env.assertEqual(env.cmd("bits.max", "mm"), None) 