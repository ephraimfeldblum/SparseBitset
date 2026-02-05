from RLTest import Env


def test_node32_fill_and_compact(env: Env):
    key = "n32_compact"
    # choose three clusters at Node32 granularity (each cluster has 2^16 = 65536 bits)
    base_a = 1 * 65536
    base_b = 3 * 65536
    base_c = 5 * 65536

    # create min in A
    env.assertEqual(env.cmd("BITS.INSERT", key, base_a), 1)
    # create max in C
    env.assertEqual(env.cmd("BITS.INSERT", key, base_c), 1)
    # create one element in cluster B so cluster_data_ is materialized
    env.assertEqual(env.cmd("BITS.INSERT", key, base_b), 1)

    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 1)

    # fill cluster B completely (insert remaining 65535 elements in chunks)
    vals = list(range(base_b + 1, base_b + 65536))
    added = 0
    step = 8192
    for i in range(0, len(vals), step):
        chunk = vals[i:i + step]
        added += env.cmd("BITS.INSERT", key, *chunk)
    # we added 65535 elements beyond the initial one
    env.assertEqual(added, 65535)

    # cluster B should be compacted away (no resident clusters)
    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)

    # membership still present and counts reflect compaction
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 42), 1)
    env.assertEqual(env.cmd("BITS.COUNT", key), 65538)  # 1 in A + 65536 in B + 1 in C

    # remove one bit from the implicitly-filled cluster B; it should materialize a resident cluster
    env.assertEqual(env.cmd("BITS.REMOVE", key, base_b + 5), 1)

    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 3) # now one resident cluster again (B materialized) containing clusters for min, max, and the removed bit (in min's cluster)

    # removed bit is absent
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 5), 0)
    # count reflects 65537 bits remain
    env.assertEqual(env.cmd("BITS.COUNT", key), 65537)

    # successor and predecessor inside the cluster behave correctly
    env.assertEqual(env.cmd("BITS.SUCCESSOR", key, base_b + 4), base_b + 6)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", key, base_b + 6), base_b + 4)


def test_node32_persistence_compaction(env: Env):
    key = "n32_compact_persist"
    base_a = 1 * 65536
    base_b = 3 * 65536
    base_c = 5 * 65536

    # create min in A and max in C
    env.assertEqual(env.cmd("BITS.INSERT", key, base_a), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key, base_c), 1)
    # create one element in cluster B so cluster_data_ is materialized
    env.assertEqual(env.cmd("BITS.INSERT", key, base_b), 1)

    # fill cluster B completely
    vals = list(range(base_b + 1, base_b + 65536))
    step = 8192
    for i in range(0, len(vals), step):
        chunk = vals[i:i + step]
        env.cmd("BITS.INSERT", key, *chunk)

    # cluster B should be compacted away
    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)

    # persist and reload
    env.dumpAndReload()

    # after reload compaction should remain
    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)

    # membership and counts survive
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 42), 1)
    env.assertEqual(env.cmd("BITS.COUNT", key), 65538)

    # remove one bit from cluster B; it should materialize a resident cluster
    env.assertEqual(env.cmd("BITS.REMOVE", key, base_b + 256 + 5), 1)
    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 4) # now one resident cluster again (B materialized) containing clusters for min, max, and the removed bit
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 256 + 5), 0)
    env.assertEqual(env.cmd("BITS.COUNT", key), 65537)
