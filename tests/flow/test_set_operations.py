from RLTest import Env


def test_set_operations(env: Env):
    env.cmd("BITS.INSERT", "s1", 1, 2, 3, 4)
    env.cmd("BITS.INSERT", "s2", 3, 4, 5, 6)

    # union into dest
    env.assertEqual(env.cmd("BITS.OP", "OR", "u", "s1", "s2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "u"), [1, 2, 3, 4, 5, 6])

    # intersection
    env.assertEqual(env.cmd("BITS.OP", "AND", "i", "s1", "s2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "i"), [3, 4])

    # diff (xor)
    env.assertEqual(env.cmd("BITS.OP", "XOR", "d", "s1", "s2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "d"), [1, 2, 5, 6])

def test_n16_set_operations(env: Env):
    src1 = [352, 194, 325, 327, 138, 236, 273, 83, 373, 125, 87, 473, 285, 311]
    src2 = [135, 395, 274, 148, 22, 150, 23, 282, 413, 35, 295, 40, 424, 309, 437, 441, 185, 443, 316, 63, 193, 321, 453, 338, 472, 98, 360, 235, 498, 116, 373, 119]
    env.cmd("BITS.INSERT", "n16_set1", *src1)
    env.cmd("BITS.INSERT", "n16_set2", *src2)

    dest_or = sorted(set(src1).union(set(src2)))
    dest_and = sorted(set(src1).intersection(set(src2)))
    dest_xor = sorted(set(src1).symmetric_difference(set(src2)))

    # union into dest
    env.cmd("BITS.OP", "OR", "n16_union", "n16_set1", "n16_set2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "n16_union"), dest_or)

    # intersection
    env.cmd("BITS.OP", "AND", "n16_intersection", "n16_set1", "n16_set2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "n16_intersection"), dest_and)

    # diff (xor)
    env.cmd("BITS.OP", "XOR", "n16_diff", "n16_set1", "n16_set2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "n16_diff"), dest_xor)     

def test_bitop_error_cases(env: Env):
    """Test error cases for BITS.OP command"""

    # Test wrong arity
    env.expect("BITS.OP").error().contains("wrong number of arguments")
    env.expect("BITS.OP", "OR").error().contains("wrong number of arguments")
    env.expect("BITS.OP", "OR", "dest").error().contains("wrong number of arguments")

    # Test invalid operation
    env.expect("BITS.OP", "INVALID", "dest", "src").error().contains("syntax error, expected AND, OR, or XOR")
    env.expect("BITS.OP", "NOT", "dest", "src").error().contains("syntax error, expected AND, OR, or XOR")

    # Test case insensitive operations
    env.cmd("BITS.INSERT", "test1", 1, 2)
    env.cmd("BITS.INSERT", "test2", 2, 3)

    env.assertEqual(env.cmd("BITS.OP", "or", "result1", "test1", "test2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "result1"), [1, 2, 3])

    env.assertEqual(env.cmd("BITS.OP", "And", "result2", "test1", "test2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "result2"), [2])

    env.assertEqual(env.cmd("BITS.OP", "XoR", "result3", "test1", "test2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "result3"), [1, 3])


def test_bitop_with_nonexistent_keys(env: Env):
    """Test BITS.OP with non-existent keys (should be treated as empty)"""

    env.cmd("BITS.INSERT", "existing", 1, 2, 3)

    # OR with non-existent key
    env.assertEqual(env.cmd("BITS.OP", "OR", "or_result", "existing", "nonexistent"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "or_result"), [1, 2, 3])

    # AND with non-existent key (should result in empty set)
    env.assertEqual(env.cmd("BITS.OP", "AND", "and_result", "existing", "nonexistent"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "and_result"), [])

    # XOR with non-existent key
    env.assertEqual(env.cmd("BITS.OP", "XOR", "xor_result", "existing", "nonexistent"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "xor_result"), [1, 2, 3])

    # All non-existent keys
    env.assertEqual(env.cmd("BITS.OP", "OR", "empty_or", "nonexistent1", "nonexistent2"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "empty_or"), [])


def test_bitop_multiple_sources(env: Env):
    """Test BITS.OP with multiple source keys"""

    env.cmd("BITS.INSERT", "set1", 1, 2)
    env.cmd("BITS.INSERT", "set2", 2, 3)
    env.cmd("BITS.INSERT", "set3", 3, 4)

    # OR with multiple sources
    env.assertEqual(env.cmd("BITS.OP", "OR", "multi_or", "set1", "set2", "set3"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "multi_or"), [1, 2, 3, 4])

    # AND with multiple sources
    env.assertEqual(env.cmd("BITS.OP", "AND", "multi_and", "set1", "set2", "set3"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "multi_and"), [])

    # XOR with multiple sources (1,2^2,3^3,4 = 1,4)
    env.assertEqual(env.cmd("BITS.OP", "XOR", "multi_xor", "set1", "set2", "set3"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "multi_xor"), [1, 4])
