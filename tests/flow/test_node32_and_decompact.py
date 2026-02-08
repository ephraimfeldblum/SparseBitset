from RLTest import Env


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
