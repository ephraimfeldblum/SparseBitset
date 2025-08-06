from RLTest import Env


def test_toarray_order(env: Env):
    env.cmd("BITS.INSERT", "arr", 100, 1, 50)
    expected = [1, 50, 100]
    env.assertEqual(env.cmd("BITS.TOARRAY", "arr"), expected)

    env.cmd("BITS.REMOVE", "arr", 50)
    env.assertEqual(env.cmd("BITS.TOARRAY", "arr"), [1, 100]) 