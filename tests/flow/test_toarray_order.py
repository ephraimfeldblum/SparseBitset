from RLTest import Env


def test_toarray_order(env: Env):
    env.cmd("bits.insert", "arr", 100, 1, 50)
    expected = [1, 50, 100]
    env.assertEqual(env.cmd("bits.toarray", "arr"), expected)

    env.cmd("bits.remove", "arr", 50)
    env.assertEqual(env.cmd("bits.toarray", "arr"), [1, 100]) 