from RLTest import Env

N = 65536


def test_node32_and_decompact(env: Env):
    key_full = "n32_full"
    key_partial = "n32_partial"
    dest = "n32_and"
    base_a = 1 * 65536
    base_b = 3 * 65536
    base_c = 5 * 65536

    # create min in A and max in C, then materialize and fill cluster B completely
    env.assertEqual(env.cmd("BITS.INSERT", key_full, base_a), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key_full, base_c), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key_full, base_b), 1)

    vals = list(range(base_b + 1, base_b + 65536))
    added = 0
    step = 8192
    for i in range(0, len(vals), step):
        chunk = vals[i:i + step]
        added += env.cmd("BITS.INSERT", key_full, *chunk)
    env.assertEqual(added, 65535)

    # cluster B should now be compacted away (implicit)
    info = env.cmd("BITS.INFO", key_full)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)

    # create a small partial cluster in key_partial inside cluster B
    env.assertEqual(env.cmd("BITS.INSERT", key_partial, base_b + 10), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key_partial, base_b + 200), 1)

    # AND should decompact the cluster and result in the partial set
    res = env.cmd("BITS.OP", "AND", dest, key_full, key_partial)
    env.assertGreater(res, 0)
    env.assertEqual(env.cmd("BITS.TOARRAY", dest), sorted([base_b + 10, base_b + 200]))

    info = env.cmd("BITS.INFO", dest)
    info_map = dict(zip(info[::2], info[1::2]))
    # resulting node contains exactly the two intersection values as min/max and therefore has no resident clusters
    env.assertEqual(info_map[b'total_clusters'], 0)
    env.assertEqual(env.cmd("BITS.COUNT", dest), 2)
    env.assertEqual(env.cmd("BITS.MIN", dest), base_b + 10)
    env.assertEqual(env.cmd("BITS.MAX", dest), base_b + 200)


def test_node32_and_decompact_inplace(env: Env):
    key_full = "n32_full_inplace"
    key_partial = "n32_partial_inplace"
    base_a = 1 * 65536
    base_b = 3 * 65536
    base_c = 5 * 65536

    # create min in A and max in C, then materialize and fill cluster B completely
    env.assertEqual(env.cmd("BITS.INSERT", key_full, base_a), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key_full, base_c), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key_full, base_b), 1)

    vals = list(range(base_b + 1, base_b + 65536))
    added = 0
    step = 8192
    for i in range(0, len(vals), step):
        chunk = vals[i:i + step]
        added += env.cmd("BITS.INSERT", key_full, *chunk)
    env.assertEqual(added, 65535)

    # cluster B should now be compacted away (implicit)
    info = env.cmd("BITS.INFO", key_full)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)

    # create key_partial with all bits in B except one missing element
    vals2 = list(range(base_b, base_b + 65536))
    vals2.remove(base_b + 5)
    added = 0
    for i in range(0, len(vals2), step):
        chunk = vals2[i:i + step]
        added += env.cmd("BITS.INSERT", key_partial, *chunk)
    # verify we inserted 65535 elements in B
    env.assertEqual(added, 65535)

    # perform in-place AND into key_full; this should decompact B into a resident cluster
    res = env.cmd("BITS.OP", "AND", key_full, key_full, key_partial)
    env.assertGreater(res, 0)

    # now key_full should have resident clusters because B is no longer implicitly full
    info = env.cmd("BITS.INFO", key_full)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertGreater(info_map[b'total_clusters'], 0)

    # the missing element should be absent, and counts should reflect the partial cluster (65535 bits in B)
    env.assertEqual(env.cmd("BITS.GET", key_full, base_b + 5), 0)
    env.assertEqual(env.cmd("BITS.COUNT", key_full), 65535)


def test_or_compaction_merge(env: Env):
    a = "or_comp_a"
    b = "or_comp_b"
    dest = "or_comp_dest"

    base_a = 1 * N
    base_b = 3 * N
    base_c = 5 * N

    env.cmd("BITS.INSERT", a, base_a)
    env.cmd("BITS.INSERT", a, *[base_b + i for i in range(0, N // 2)])

    env.cmd("BITS.INSERT", b, base_c)
    env.cmd("BITS.INSERT", b, *[base_b + i for i in range(N // 2, N)])

    env.cmd("BITS.OP", "OR", dest, a, b)
    info = env.cmd("BITS.INFO", dest)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)
    env.assertEqual(env.cmd("BITS.COUNT", dest), N + 2)
    env.assertEqual(env.cmd("BITS.GET", dest, base_b + 42), 1)


def test_or_with_compacted_source(env: Env):
    src = "comp_src"
    other = "comp_other"
    dest = "comp_or"

    base_a = 1 * N
    base_b = 3 * N
    base_c = 5 * N

    env.cmd("BITS.INSERT", src, base_a)
    env.cmd("BITS.INSERT", src, base_c)
    env.cmd("BITS.INSERT", src, *range(base_b, base_b + N))

    env.cmd("BITS.INSERT", other, base_c + 1, base_b + 13, base_b + 37)

    env.cmd("BITS.OP", "OR", dest, src, other)
    expected = sorted(set(env.cmd("BITS.TOARRAY", src)) | set(env.cmd("BITS.TOARRAY", other)))
    env.assertEqual(env.cmd("BITS.TOARRAY", dest), expected)


def test_and_with_compacted_source(env: Env):
    src = "and_comp_src"
    other = "and_comp_other"
    dest = "and_comp_dest"

    base_a = 1 * N
    base_b = 3 * N
    base_c = 5 * N

    env.cmd("BITS.INSERT", src, base_a)
    env.cmd("BITS.INSERT", src, base_c)
    env.cmd("BITS.INSERT", src, *range(base_b, base_b + N))

    env.cmd("BITS.INSERT", other, base_c + 1, base_b + 13, base_b + 37)

    env.cmd("BITS.OP", "AND", dest, src, other)
    env.assertEqual(env.cmd("BITS.TOARRAY", dest), [base_b + 13, base_b + 37])


def test_and_compaction_merge(env: Env):
    a = "and_comp_a"
    b = "and_comp_b"
    dest = "and_comp_dest2"

    base_a = 1 * N
    base_b = 3 * N
    base_c = 5 * N

    env.cmd("BITS.INSERT", a, base_a - 1)
    env.cmd("BITS.INSERT", a, base_a)
    env.cmd("BITS.INSERT", a, base_c - 1)
    env.cmd("BITS.INSERT", a, base_c)
    env.cmd("BITS.INSERT", a, *range(base_b, base_b + N))

    env.cmd("BITS.INSERT", b, base_a)
    env.cmd("BITS.INSERT", b, base_a + 1)
    env.cmd("BITS.INSERT", b, base_c)
    env.cmd("BITS.INSERT", b, base_c + 1)
    env.cmd("BITS.INSERT", b, *range(base_b, base_b + N))

    env.cmd("BITS.OP", "AND", dest, a, b)
    info = env.cmd("BITS.INFO", dest)
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: base_a,
    #   max: base_c,
    #   summary: (Node16){
    #       min: 3,
    #       max: 3,
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 0)
    env.assertEqual(env.cmd("BITS.COUNT", dest), N + 2)
    actual = set(env.cmd("BITS.TOARRAY", dest))
    expected = {base_a, base_c} | set(range(base_b, base_b + N))
    diff = actual ^ expected
    env.assertEqual(diff, set())


def test_node32_or_compaction(env: Env):
    """Scenario: OR creating full subnodes (Compaction)"""
    env.cmd("BITS.INSERT", "s1", 0, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 3 * N // 2))

    env.cmd("BITS.INSERT", "s2", 0, 1000000)
    env.cmd("BITS.INSERT", "s2", *range(3 * N // 2, 2 * N))

    env.cmd("BITS.OP", "OR", "dest", "s1", "s2")

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 1000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 0)
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N + 2)
    env.assertEqual(env.cmd("BITS.GET", "dest", N + 42), 1)
    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    expected = {0, 1000000} | set(range(N, 2 * N))
    diff = actual ^ expected
    env.assertEqual(diff, set())


def test_node32_and_resident_from_nonresident(env: Env):
    """Scenario: AND resulting in resident subnode from non-resident (implicitly filled)"""
    env.cmd("BITS.INSERT", "s1", 0, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 0, 1000000)
    env.cmd("BITS.INSERT", "s2", N, N + 1, N + 2)

    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 1000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 0,
    #           max: 2,
    #           summary: (Node8)[0],
    #           unfilled: (Node8)[..],
    #           clusters: [
    #               (Node8)[1],
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 2)
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), 5)
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0, N, N + 1, N + 2, 1000000])


def test_node32_xor_resident_from_nonresident(env: Env):
    env.cmd("BITS.INSERT", "s1", 0, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 0, 1000000)
    env.cmd("BITS.INSERT", "s2", *range(N, N + 10))

    env.cmd("BITS.OP", "XOR", "dest", "s1", "s2")

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min:     N + 10,
    #   max: 2 * N - 1,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 11,
    #           max: -2,
    #           summary: (Node8)[..],
    #           unfilled: (Node8)[0,-1],
    #           clusters: [
    #               (Node8)[12..],
    #               (Node8)[..-3],
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 3)
    env.assertEqual(env.cmd("BITS.MIN", "dest"), N + 10)
    env.assertEqual(env.cmd("BITS.MAX", "dest"), 2 * N - 1)
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N - 10)
    expected = set(range(N + 10, 2 * N))
    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    diff = actual ^ expected
    env.assertEqual(diff, set())


def test_node32_nonresident_before_minimal_resident(env: Env):
    """Scenario: nonresident subnodes before the minimal resident subnode"""
    env.cmd("BITS.INSERT", "s1", 0, 2000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))
    env.cmd("BITS.INSERT", "s1", 2 * N, 2 * N + 1)

    info = env.cmd("BITS.INFO", "s1")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 2000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 2,
    #   },
    #   clusters: {
    #       2: (Node16){
    #           min: 0,
    #           max: 1,
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 1)

    env.assertEqual(env.cmd("BITS.SUCCESSOR", "s1", 1), N)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "s1", N - 1), N)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "s1", 2 * N - 1), 2 * N)


def test_node32_min_promotion_from_nonresident(env: Env):
    """Scenario: AND where min is removed, and we promote from a non-resident (full) cluster"""
    env.cmd("BITS.INSERT", "s1", 10, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 20, 1000000)
    env.cmd("BITS.INSERT", "s2", *range(N, 2 * N))

    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")

    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N + 1)
    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    expected = {20} | set(range(N, 2 * N))
    diff = actual ^ expected
    env.assertEqual(diff, set())

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: N,
    #   max: 1000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 1,
    #           max: -1,
    #           summary: (Node8)[..],
    #           unfilled: (Node8)[0,-1],
    #           clusters: [
    #               (Node8)[2..],
    #               (Node8)[..-2],
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 3)


def test_node32_max_promotion_from_nonresident(env: Env):
    """Scenario: AND where max is removed, and we promote from a non-resident (full) cluster"""
    env.cmd("BITS.INSERT", "s1", 0, 990000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 0, 980000)
    env.cmd("BITS.INSERT", "s2", *range(N, 2 * N))

    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")

    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N + 1)
    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    expected = {0} | set(range(N, 2 * N))
    diff = actual ^ expected
    env.assertEqual(diff, set())

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 2 * N - 1,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 0,
    #           max: -2,
    #           summary: (Node8)[..],
    #           unfilled: (Node8)[0,-1],
    #           clusters: [
    #               (Node8)[1..],
    #               (Node8)[..-3],
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 3)


def test_node32_complex_resident_nonresident_mix(env: Env):
    """Scenario: complex mix of resident and non-resident subnodes in set ops"""
    env.cmd("BITS.INSERT", "s1", 0, 2000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))
    env.cmd("BITS.INSERT", "s1", 2 * N, 2 * N + 1)

    env.cmd("BITS.INSERT", "s2", 0, 2000000)
    env.cmd("BITS.INSERT", "s2", N, N + 1)
    env.cmd("BITS.INSERT", "s2", *range(2 * N, 3 * N))

    env.cmd("BITS.OP", "OR", "res_or", "s1", "s2")
    env.assertEqual(env.cmd("BITS.COUNT", "res_or"), N + N + 2)
    
    actual = set(env.cmd("BITS.TOARRAY", "res_or"))
    expected = {0, 2000000} | set(range(N, 3 * N))
    diff = actual ^ expected
    env.assertEqual(diff, set())

    info = env.cmd("BITS.INFO", "res_or")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 2000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 2,
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 0)

    env.cmd("BITS.OP", "AND", "res_and", "s1", "s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_and"), [0, N, N + 1, 2 * N, 2 * N + 1, 2000000])
    info = env.cmd("BITS.INFO", "res_and")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 2000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 2,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 0,
    #           max: 1,
    #       },
    #       2: (Node16){
    #           min: 0,
    #           max: 1,
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 2)

    env.cmd("BITS.OP", "XOR", "res_xor", "s1", "s2")
    env.assertEqual(env.cmd("BITS.COUNT", "res_xor"), 2 * N - 4)

    actual = set(env.cmd("BITS.TOARRAY", "res_xor"))
    expected = set(range(N + 2, 2 * N)) | set(range(2 * N + 2, 3 * N))
    diff = actual ^ expected
    env.assertEqual(diff, set())

    info = env.cmd("BITS.INFO", "res_xor")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: N + 2,
    #   max: 2 * N - 1,
    #   summary: (Node16){
    #       min: 1,
    #       max: 2,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 3,
    #           max: -1,
    #           summary: (Node8)[..],
    #           unfilled: (Node8)[0,-1],
    #           clusters: [
    #               (Node8)[4..],
    #               (Node8)[..-2],
    #           ],
    #       },
    #       2: (Node16){
    #           min: 2,
    #           max: -2,
    #           summary: (Node8)[..],
    #           unfilled: (Node8)[0,-1],
    #           clusters: [
    #               (Node8)[3..],
    #               (Node8)[..-3],
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 6)


def test_node32_or_desync_edge_case(env: Env):
    """Scenario: OR where a resident cluster becomes non-resident due to overlap with a full cluster,
    potentially desyncing subsequent resident clusters."""
    env.cmd("BITS.INSERT", "s1", 0, 10000000)
    env.cmd("BITS.INSERT", "s1", N + 10)
    env.cmd("BITS.INSERT", "s1", 2 * N + 20)

    env.cmd("BITS.INSERT", "s2", 0, 10000000)
    env.cmd("BITS.INSERT", "s2", *range(N, 2 * N))
    env.cmd("BITS.INSERT", "s2", 2 * N + 30)

    env.cmd("BITS.OP", "OR", "dest", "s1", "s2")

    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N + 4)
    env.assertEqual(env.cmd("BITS.GET", "dest", 2 * N + 20), 1)
    env.assertEqual(env.cmd("BITS.GET", "dest", 2 * N + 30), 1)
    env.assertEqual(env.cmd("BITS.GET", "dest", N + 10), 1)

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 10000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 2,
    #   },
    #   clusters: {
    #       2: (Node16){
    #           min: 20,
    #           max: 30,
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 1)

    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    expected = {0, 10000000} | set(range(N, 2 * N)) | {2 * N + 20, 2 * N + 30}
    diff = actual ^ expected
    env.assertEqual(diff, set())


def test_node32_xor_full_resident_mix(env: Env):
    """Scenario: XOR where one side is full and the other is resident."""
    env.cmd("BITS.INSERT", "s1", 0, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 0, 1000000)
    env.cmd("BITS.INSERT", "s2", N, N + 1, N + 2)

    env.cmd("BITS.OP", "XOR", "dest", "s1", "s2")

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: N + 3,
    #   max: 2 * N - 1,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 4,
    #           max: -2,
    #           summary: (Node8)[..],
    #           unfilled: (Node8)[0,-1],
    #           clusters: [
    #               (Node8)[5..],
    #               (Node8)[..-3],
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 3)

    env.assertEqual(env.cmd("BITS.GET", "dest", N), 0)
    env.assertEqual(env.cmd("BITS.GET", "dest", N + 3), 1)
    env.assertEqual(env.cmd("BITS.GET", "dest", 2 * N - 1), 1)

    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N - 3)
    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    expected = set(range(N, 2 * N)) - {N, N + 1, N + 2}
    diff = actual ^ expected
    env.assertEqual(diff, set())


def test_node32_and_full_resident_mix(env: Env):
    """Scenario: AND where one side is full and the other is resident."""
    env.cmd("BITS.INSERT", "s1", 0, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 0, 1000000)
    env.cmd("BITS.INSERT", "s2", N, N + 1, N + 2)

    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")

    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0, N, N + 1, N + 2, 1000000])
    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (node32){
    #   min: 0,
    #   max:1000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    #   clusters: {
    #       1: (Node16){
    #           min: 0,
    #           max: 2,
    #           summary: (Node8)[0],
    #           unfilled: (Node8)[..],
    #           clusters: [
    #               (Node8)[1]
    #           ],
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 2)


def test_node32_or_two_full_clusters(env: Env):
    """Scenario: OR where both sides have a full cluster (should stay non-resident/full)."""
    env.cmd("BITS.INSERT", "s1", 0, 1000000)
    env.cmd("BITS.INSERT", "s1", *range(N, 2 * N))

    env.cmd("BITS.INSERT", "s2", 0, 1000000)
    env.cmd("BITS.INSERT", "s2", *range(N, 2 * N))

    env.cmd("BITS.OP", "OR", "dest", "s1", "s2")

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 1000000,
    #   summary: (Node16){
    #       min: 1,
    #       max: 1,
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 0)

    env.assertEqual(env.cmd("BITS.COUNT", "dest"), N + 2)
    actual = set(env.cmd("BITS.TOARRAY", "dest"))
    expected = {0, 1000000} | set(range(N, 2 * N))
    diff = actual ^ expected
    env.assertEqual(diff, set())


def test_node32_and_promotion_desync_edge_case(env: Env):
    """Scenario: AND where a resident cluster is removed, potentially desyncing subsequent clusters."""
    env.cmd("BITS.INSERT", "s1", 0, 10000000)
    env.cmd("BITS.INSERT", "s1", N + 10)
    env.cmd("BITS.INSERT", "s1", 2 * N + 20)

    env.cmd("BITS.INSERT", "s2", 0, 10000000)
    env.cmd("BITS.INSERT", "s2", 2 * N + 20)

    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0, 2 * N + 20, 10000000])

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 0,
    #   max: 10000000,
    #   summary: (Node16){
    #       min: 2,
    #       max: 2,
    #   },
    #   clusters: {
    #       2: (Node16){
    #           min: 20,
    #           max: 20,
    #       },
    #   },
    # }
    env.assertEqual(info_map[b'total_clusters'], 1)


def test_node32_xor_promotion_desync_edge_case(env: Env):
    """Scenario: XOR where a resident cluster is removed or changed, potentially desyncing."""
    env.cmd("BITS.INSERT", "s1", 0, 10000000)
    env.cmd("BITS.INSERT", "s1", N + 10)
    env.cmd("BITS.INSERT", "s1", 2 * N + 20)

    env.cmd("BITS.INSERT", "s2", 0, 10000000)
    env.cmd("BITS.INSERT", "s2", N + 10)
    env.cmd("BITS.INSERT", "s2", 2 * N + 30)

    env.cmd("BITS.OP", "XOR", "dest", "s1", "s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [2 * N + 20, 2 * N + 30])

    info = env.cmd("BITS.INFO", "dest")
    info_map = dict(zip(info[::2], info[1::2]))
    # (Node32){
    #   min: 2 * N + 20,
    #   max: 2 * N + 30,
    # }
    env.assertEqual(info_map[b'total_clusters'], 0)
