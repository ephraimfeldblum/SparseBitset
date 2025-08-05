from RLTest import Env


def test_add_and_size(env: Env):
    env.assertEqual(env.cmd("bits.insert", "mset", 1, 5, 10), 3)
    env.assertEqual(env.cmd("bits.size", "mset"), 3)
    env.assertEqual(env.cmd("bits.contains", "mset", 5), 1)
    env.assertEqual(env.cmd("bits.contains", "mset", 7), 0)


def test_add_duplicates(env: Env):
    env.cmd("bits.insert", "dup", 2, 4)
    env.assertEqual(env.cmd("bits.insert", "dup", 2, 4), 0)
    env.assertEqual(env.cmd("bits.size", "dup"), 2)


def test_remove(env: Env):
    env.cmd("bits.insert", "rem", 1, 2, 3, 4)
    env.assertEqual(env.cmd("bits.remove", "rem", 2, 5), 1)
    env.assertEqual(env.cmd("bits.size", "rem"), 3)
    env.assertEqual(env.cmd("bits.contains", "rem", 2), 0)
    env.assertEqual(env.cmd("bits.contains", "rem", 3), 1)


def test_clear(env: Env):
    env.cmd("bits.insert", "clr", 100, 200)
    env.assertEqual(env.cmd("bits.clear", "clr"), b"OK")
    env.assertEqual(env.cmd("bits.size", "clr"), 0) 