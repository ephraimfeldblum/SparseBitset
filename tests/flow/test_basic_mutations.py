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


def test_non_existent_key(env: Env):
    env.assertEqual(env.cmd("BITS.GET", "nonexistent", 10), 0)
    env.assertEqual(env.cmd("BITS.COUNT", "nonexistent"), 0)
    env.assertEqual(env.cmd("BITS.REMOVE", "nonexistent", 10), 0)
    env.assertEqual(env.cmd("BITS.CLEAR", "nonexistent"), b"OK")


def test_large_range_insert(env: Env):
    key = "large_range"
    elements = [1, 100, 10000, 1000000]
    env.assertEqual(env.cmd("BITS.INSERT", key, *elements), 4)
    env.assertEqual(env.cmd("BITS.COUNT", key), 4)
    for e in elements:
        env.assertEqual(env.cmd("BITS.GET", key, e), 1)
    env.assertEqual(env.cmd("BITS.GET", key, 50), 0)


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


def test_min_max_non_existent(env: Env):
    env.assertEqual(env.cmd("BITS.MIN", "nonexistent"), None)
    env.assertEqual(env.cmd("BITS.MAX", "nonexistent"), None)

def test_membership_and_size(env: Env):
    env.assertEqual(env.cmd("BITS.INSERT", "ms", 1, 2, 3), 3)
    env.assertEqual(env.cmd("BITS.COUNT", "ms"), 3)
    env.assertEqual(env.cmd("BITS.GET", "ms", 2), 1)
    env.assertEqual(env.cmd("BITS.GET", "ms", 4), 0)

    # remove one element
    env.assertEqual(env.cmd("BITS.REMOVE", "ms", 2), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "ms"), 2)
    env.assertEqual(env.cmd("BITS.GET", "ms", 2), 0) 
