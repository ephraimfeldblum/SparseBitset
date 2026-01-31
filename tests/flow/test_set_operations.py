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

    or_expected = sorted(set(src1).union(set(src2)))
    and_expected = sorted(set(src1).intersection(set(src2)))
    xor_expected = sorted(set(src1).symmetric_difference(set(src2)))

    env.cmd("BITS.OP", "OR", "n16_union", "n16_set1", "n16_set2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "n16_union"), or_expected)

    env.cmd("BITS.OP", "AND", "n16_intersection", "n16_set1", "n16_set2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "n16_intersection"), and_expected)

    env.cmd("BITS.OP", "XOR", "n16_diff", "n16_set1", "n16_set2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "n16_diff"), xor_expected)     


def test_minmax(env: Env):
    src1 = [3, 16, 412]
    src2 = [16, 412]
    env.cmd("BITS.INSERT", "A", *src1)
    env.cmd("BITS.INSERT", "B", *src2)
    
    or_expected = sorted(set(src1).union(set(src2)))
    and_expected = sorted(set(src1).intersection(set(src2)))
    xor_expected = sorted(set(src1).symmetric_difference(set(src2)))

    env.cmd("BITS.OP", "OR", "dest", "A", "B")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), or_expected)

    env.cmd("BITS.OP", "AND", "dest", "A", "B")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), and_expected)

    env.cmd("BITS.OP", "XOR", "dest", "A", "B")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), xor_expected)     


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


def test_or_compaction_merge(env: Env):
    # Make two sets where combined cluster becomes full and should compact
    a = "or_comp_a"
    b = "or_comp_b"
    dest = "or_comp_dest"

    base_a = 1 * 256
    base_b = 3 * 256
    base_c = 5 * 256

    # ensure min and max live outside target cluster (B)
    env.cmd("BITS.INSERT", a, base_a)
    env.cmd("BITS.INSERT", a, *[base_b + i for i in range(0, 128)])

    env.cmd("BITS.INSERT", b, base_c)
    env.cmd("BITS.INSERT", b, *[base_b + i for i in range(128, 256)])

    # OR them; result should compact cluster B (become implicit)
    env.cmd("BITS.OP", "OR", dest, a, b)
    info = env.cmd("BITS.INFO", dest)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)
    # membership and counts
    env.assertEqual(env.cmd("BITS.COUNT", dest), 258)
    env.assertEqual(env.cmd("BITS.GET", dest, base_b + 42), 1)


def test_or_with_compacted_source(env: Env):
    # OR where one source already has a compacted (implicitly filled) cluster
    src = "comp_src"
    other = "comp_other"
    dest = "comp_or"

    base_a = 1 * 256
    base_b = 3 * 256
    base_c = 5 * 256

    # src has compacted cluster B (fill it)
    env.cmd("BITS.INSERT", src, base_a)
    env.cmd("BITS.INSERT", src, base_c)
    env.cmd("BITS.INSERT", src, base_b)
    env.cmd("BITS.INSERT", src, *list(range(base_b, base_b + 256))[1:])

    # other has some values in B and some elsewhere
    env.cmd("BITS.INSERT", other, base_c + 1, base_b + 13, base_b + 37)

    env.cmd("BITS.OP", "OR", dest, src, other)
    # result should contain union
    expected = sorted(set(env.cmd("BITS.TOARRAY", src)) | set(env.cmd("BITS.TOARRAY", other)))
    env.assertEqual(env.cmd("BITS.TOARRAY", dest), expected)


def test_node32_and_corner_cases(env: Env):
    """Exercise values >= 2^16 to force node32 and cover edge cases."""
    # large values around 2^16 boundary and much larger values
    a = [0, 1, 65535, 65536, 65537, 70000, 1 << 20]
    b = [65535, 65536, 80000, (1 << 20) + 1]

    env.cmd("BITS.INSERT", "n32_a", *a)
    env.cmd("BITS.INSERT", "n32_b", *b)

    # OR
    env.assertGreater(env.cmd("BITS.OP", "OR", "n32_or", "n32_a", "n32_b"), 0)
    expected_or = sorted(set(a).union(set(b)))
    env.assertEqual(env.cmd("BITS.TOARRAY", "n32_or"), expected_or)
    env.assertEqual(env.cmd("BITS.COUNT", "n32_or"), len(expected_or))

    # AND
    res_and = env.cmd("BITS.OP", "AND", "n32_and", "n32_a", "n32_b")
    expected_and = sorted(set(a).intersection(set(b)))
    if not expected_and:
        env.assertEqual(res_and, 0)
    else:
        env.assertGreater(res_and, 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "n32_and"), expected_and)

    # XOR of identical large sets -> empty
    env.cmd("BITS.INSERT", "n32_c1", 65536, 70000)
    env.cmd("BITS.INSERT", "n32_c2", 65536, 70000)
    res_xor = env.cmd("BITS.OP", "XOR", "n32_xor", "n32_c1", "n32_c2")
    env.assertEqual(res_xor, 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "n32_xor"), [])

    # OR with a non-existent key should be equivalent to the existing set
    res_mixed = env.cmd("BITS.OP", "OR", "n32_mixed", "n32_a", "nonexistent_key")
    env.assertGreater(res_mixed, 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "n32_mixed"), sorted(set(a)))

    # AND with a non-existent key should produce empty
    env.assertEqual(env.cmd("BITS.OP", "AND", "n32_and_empty", "n32_a", "nonexistent_key"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "n32_and_empty"), [])


def test_set_ops_overlapping_ranges_and_emptying(env: Env):
    # overlapping ranges, disjoint ranges, and emptying via removes
    low = list(range(10, 20))
    mid = list(range(15, 30))
    high = [100000, 100001, 200000]

    env.cmd("BITS.INSERT", "low", *low)
    env.cmd("BITS.INSERT", "mid", *mid)
    env.cmd("BITS.INSERT", "high", *high)

    # OR low and mid
    env.assertGreater(env.cmd("BITS.OP", "OR", "low_mid_or", "low", "mid"), 0)
    expected_low_mid_or = sorted(set(low).union(set(mid)))
    env.assertEqual(env.cmd("BITS.TOARRAY", "low_mid_or"), expected_low_mid_or)

    # AND low and mid
    env.assertGreater(env.cmd("BITS.OP", "AND", "low_mid_and", "low", "mid"), 0)
    expected_low_mid_and = sorted(set(low).intersection(set(mid)))
    env.assertEqual(env.cmd("BITS.TOARRAY", "low_mid_and"), expected_low_mid_and)

    # XOR low and mid
    env.assertGreater(env.cmd("BITS.OP", "XOR", "low_mid_xor", "low", "mid"), 0)
    expected_low_mid_xor = sorted(set(low).symmetric_difference(set(mid)))
    env.assertEqual(env.cmd("BITS.TOARRAY", "low_mid_xor"), expected_low_mid_xor)

    # OR with disjoint high
    env.assertGreater(env.cmd("BITS.OP", "OR", "all_or", "low_mid_or", "high"), 0)
    expected_all_or = sorted(set(expected_low_mid_or).union(set(high)))
    env.assertEqual(env.cmd("BITS.TOARRAY", "all_or"), expected_all_or)

    # Removing elements to empty a set
    env.cmd("BITS.REMOVE", "low", *low)
    env.assertEqual(env.cmd("BITS.COUNT", "low"), 0)
    # AND of empty and non-empty should be empty
    env.assertEqual(env.cmd("BITS.OP", "AND", "empty_and_high", "low", "high"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "empty_and_high"), [])


def test_bitop_dest_is_source(env: Env):
    """Test BITS.OP where destination is one of the sources"""
    env.cmd("BITS.INSERT", "s1", 1, 2)
    env.cmd("BITS.INSERT", "s2", 2, 3)

    # OR into s1
    env.assertEqual(env.cmd("BITS.OP", "OR", "s1", "s1", "s2"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "s1"), [1, 2, 3])

    # AND into s1
    env.cmd("BITS.INSERT", "s3", 3, 4)
    env.assertEqual(env.cmd("BITS.OP", "AND", "s1", "s1", "s3"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "s1"), [3])

    # XOR into s1
    env.cmd("BITS.INSERT", "s4", 3, 5)
    env.assertEqual(env.cmd("BITS.OP", "XOR", "s1", "s1", "s4"), 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "s1"), [5])


def test_set_ops_type_promotion(env: Env):
    """Test set operations between different node types (Node8, Node16, Node32)"""
    # Node8 (0-255)
    env.cmd("BITS.INSERT", "s8", 1, 10, 200)
    # Node16 (256-65535)
    env.cmd("BITS.INSERT", "s16", 300, 1000, 60000)
    # Node32 (> 65535)
    env.cmd("BITS.INSERT", "s32", 70000, 100000, 1000000)

    # OR s8 | s16 -> should promote to Node16
    env.cmd("BITS.OP", "OR", "res8_16", "s8", "s16")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res8_16"), [1, 10, 200, 300, 1000, 60000])

    # OR s16 | s32 -> should promote to Node32
    env.cmd("BITS.OP", "OR", "res16_32", "s16", "s32")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res16_32"), [300, 1000, 60000, 70000, 100000, 1000000])

    # AND s8 & s32 -> should be empty
    env.assertEqual(env.cmd("BITS.OP", "AND", "res8_32", "s8", "s32"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "res8_32"), [])

    # XOR s8 ^ s32 -> should contain both
    env.cmd("BITS.OP", "XOR", "res8_32_xor", "s8", "s32")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res8_32_xor"), [1, 10, 200, 70000, 100000, 1000000])


def test_set_ops_boundary_overlaps(env: Env):
    """Test set operations where min/max boundaries overlap in interesting ways"""
    # Set A: [10, 20, 30]
    env.cmd("BITS.INSERT", "A_overlap", 10, 20, 30)
    # Set B: [5, 10, 25, 30, 35]
    env.cmd("BITS.INSERT", "B_overlap", 5, 10, 25, 30, 35)

    # Intersection: [10, 30] - notice these are min/max of A and middle values of B
    env.cmd("BITS.OP", "AND", "inter_overlap", "A_overlap", "B_overlap")
    env.assertEqual(env.cmd("BITS.TOARRAY", "inter_overlap"), [10, 30])

    # XOR: [5, 20, 25, 35]
    env.cmd("BITS.OP", "XOR", "xor_overlap", "A_overlap", "B_overlap")
    env.assertEqual(env.cmd("BITS.TOARRAY", "xor_overlap"), [5, 20, 25, 35])


def test_set_ops_identical_sets(env: Env):
    """Test set operations between identical sets"""
    vals = [1, 100, 10000, 1000000]
    env.cmd("BITS.INSERT", "ident_s1", *vals)
    env.cmd("BITS.INSERT", "ident_s2", *vals)

    # AND identical -> same set
    env.cmd("BITS.OP", "AND", "res_and_ident", "ident_s1", "ident_s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_and_ident"), vals)

    # OR identical -> same set
    env.cmd("BITS.OP", "OR", "res_or_ident", "ident_s1", "ident_s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_or_ident"), vals)

    # XOR identical -> empty set
    env.assertEqual(env.cmd("BITS.OP", "XOR", "res_xor_ident", "ident_s1", "ident_s2"), 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_xor_ident"), [])


def test_set_ops_subset_superset(env: Env):
    """Test set operations where one is a subset of another"""
    subset = [10, 20]
    superset = [5, 10, 15, 20, 25]
    env.cmd("BITS.INSERT", "sub_test", *subset)
    env.cmd("BITS.INSERT", "super_test", *superset)

    # AND -> subset
    env.cmd("BITS.OP", "AND", "res_and_sub", "sub_test", "super_test")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_and_sub"), subset)

    # OR -> superset
    env.cmd("BITS.OP", "OR", "res_or_sub", "sub_test", "super_test")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_or_sub"), superset)

    # XOR -> elements only in super
    env.cmd("BITS.OP", "XOR", "res_xor_sub", "sub_test", "super_test")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_xor_sub"), [5, 15, 25])


def test_set_ops_many_large_values(env: Env):
    """Test set operations with large number of values to trigger node growth/FAM logic"""
    s1_vals = []
    s2_vals = []
    for i in range(100):
        s1_vals.append(i * 256 + 1)
        s2_vals.append(i * 256 + 1)
        s1_vals.append(i * 256 + 2)
        s2_vals.append(i * 256 + 3)

    env.cmd("BITS.INSERT", "many_s1", *s1_vals)
    env.cmd("BITS.INSERT", "many_s2", *s2_vals)

    # AND
    env.cmd("BITS.OP", "AND", "res_and_many", "many_s1", "many_s2")
    expected_and = sorted(set(s1_vals) & set(s2_vals))
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_and_many"), expected_and)

    # OR
    env.cmd("BITS.OP", "OR", "res_or_many", "many_s1", "many_s2")
    expected_or = sorted(set(s1_vals) | set(s2_vals))
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_or_many"), expected_or)

    # XOR
    env.cmd("BITS.OP", "XOR", "res_xor_many", "many_s1", "many_s2")
    expected_xor = sorted(set(s1_vals) ^ set(s2_vals))
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_xor_many"), expected_xor)
