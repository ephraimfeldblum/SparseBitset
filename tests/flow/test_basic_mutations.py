from RLTest import Env


def test_add_and_size(env: Env):
    env.assertEqual(env.cmd("BITS.INSERT", "mset", 1, 5, 10), 3)
    env.assertEqual(env.cmd("BITS.COUNT", "mset"), 3)
    env.assertEqual(env.cmd("BITS.GET", "mset", 5), 1)
    env.assertEqual(env.cmd("BITS.GET", "mset", 7), 0)


def test_add_duplicates(env: Env):
    env.cmd("BITS.INSERT", "dup", 2, 4)
    env.assertEqual(env.cmd("BITS.INSERT", "dup", 2, 4), 0)
    env.assertEqual(env.cmd("BITS.COUNT", "dup"), 2)


def test_remove(env: Env):
    env.cmd("BITS.INSERT", "rem", 1, 2, 3, 4)
    env.assertEqual(env.cmd("BITS.REMOVE", "rem", 2, 5), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "rem"), 3)
    env.assertEqual(env.cmd("BITS.GET", "rem", 2), 0)
    env.assertEqual(env.cmd("BITS.GET", "rem", 3), 1)


def test_clear(env: Env):
    env.cmd("BITS.INSERT", "clr", 100, 200)
    env.assertEqual(env.cmd("BITS.CLEAR", "clr"), b"OK")
    env.assertEqual(env.cmd("BITS.COUNT", "clr"), 0)


def test_set(env: Env):
    key = "set_key"
    env.assertEqual(env.cmd("BITS.SET", key, 10, 1), 0)
    env.assertEqual(env.cmd("BITS.GET", key, 10), 1)
    env.assertEqual(env.cmd("BITS.SET", key, 10, 1), 1)
    env.assertEqual(env.cmd("BITS.SET", key, 10, 0), 1)
    env.assertEqual(env.cmd("BITS.GET", key, 10), 0)
    env.assertEqual(env.cmd("BITS.COUNT", key), 0)