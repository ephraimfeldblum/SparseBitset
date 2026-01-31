from RLTest import Env


def test_fill_and_compact(env: Env):
    key = "compact"
    # pick three clusters: A < B < C. min stored in A, max stored in C, target cluster is B
    base_a = 1 * 256
    base_b = 3 * 256
    base_c = 5 * 256

    # create min in A
    env.assertEqual(env.cmd("BITS.INSERT", key, base_a), 1)
    # create max in C
    env.assertEqual(env.cmd("BITS.INSERT", key, base_c), 1)
    # create one element in cluster B so cluster_data_ is materialized
    env.assertEqual(env.cmd("BITS.INSERT", key, base_b), 1)

    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    # one resident cluster B initially
    env.assertEqual(info_map[b'total_clusters'], 1)

    # fill cluster B completely
    vals = list(range(base_b, base_b + 256))
    env.assertEqual(env.cmd("BITS.INSERT", key, *vals), 255)

    # cluster B should be compacted away (no resident clusters)
    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)

    # membership still present and counts reflect compaction
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 42), 1)
    env.assertEqual(env.cmd("BITS.COUNT", key), 258)  # 1 in A + 256 in B + 1 in C

    # remove one bit from the implicitly-filled cluster B; it should materialize a resident cluster
    env.assertEqual(env.cmd("BITS.REMOVE", key, base_b + 5), 1)

    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    # now one resident cluster again (B materialized)
    env.assertEqual(info_map[b'total_clusters'], 1)

    # removed bit is absent
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 5), 0)
    # count reflects 257 bits remain
    env.assertEqual(env.cmd("BITS.COUNT", key), 257)

    # successor and predecessor inside the cluster behave correctly
    env.assertEqual(env.cmd("BITS.SUCCESSOR", key, base_b + 4), base_b + 6)
    env.assertEqual(env.cmd("BITS.PREDECESSOR", key, base_b + 6), base_b + 4)


def test_persistence_compaction(env: Env):
    key = "compact_persist"
    # pick three clusters: A < B < C. min stored in A, max stored in C, target cluster is B
    base_a = 1 * 256
    base_b = 3 * 256
    base_c = 5 * 256

    # create min in A and max in C
    env.assertEqual(env.cmd("BITS.INSERT", key, base_a), 1)
    env.assertEqual(env.cmd("BITS.INSERT", key, base_c), 1)
    # create one element in cluster B so cluster_data_ is materialized
    env.assertEqual(env.cmd("BITS.INSERT", key, base_b), 1)

    # fill cluster B completely
    vals = list(range(base_b, base_b + 256))
    env.assertEqual(env.cmd("BITS.INSERT", key, *vals), 255)

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
    env.assertEqual(env.cmd("BITS.COUNT", key), 258)

    # remove one bit from cluster B; it should materialize a resident cluster
    env.assertEqual(env.cmd("BITS.REMOVE", key, base_b + 5), 1)
    info = env.cmd("BITS.INFO", key)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 1)
    env.assertEqual(env.cmd("BITS.GET", key, base_b + 5), 0)
    env.assertEqual(env.cmd("BITS.COUNT", key), 257)
