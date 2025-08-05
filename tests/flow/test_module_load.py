from RLTest import Env


def test_module_load(env: Env):
    """Verify that the bitset module is loaded and basic commands work."""

    # Ensure Redis is up
    env.cmd("PING")

    # Confirm our module is listed
    modules = env.cmd("MODULE", "LIST")
    print(modules)
    env.assertTrue(any(mod[1] == b"sparsebit" for mod in modules), message="bitset module missing in MODULE LIST")

    # Basic operation: insert and size
    env.assertEqual(env.cmd("BITS.INSERT", "myset", 1, 2, 3), 3)
    env.assertEqual(env.cmd("BITS.SIZE", "myset"), 3) 