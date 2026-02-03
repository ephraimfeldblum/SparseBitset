from RLTest import Env


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
    env.cmd("BITS.INSERT", src, *list(range(base_b, base_b + 256)))

    # other has some values in B and some elsewhere
    env.cmd("BITS.INSERT", other, base_c + 1, base_b + 13, base_b + 37)

    env.cmd("BITS.OP", "OR", dest, src, other)
    # result should contain union
    expected = sorted(set(env.cmd("BITS.TOARRAY", src)) | set(env.cmd("BITS.TOARRAY", other)))
    env.assertEqual(env.cmd("BITS.TOARRAY", dest), expected)


def test_and_with_compacted_source(env: Env):
    # AND where one source has a compacted (implicitly filled) cluster
    src = "and_comp_src"
    other = "and_comp_other"
    dest = "and_comp_dest"

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

    env.cmd("BITS.OP", "AND", dest, src, other)
    # result should contain only the values present in other (since src cluster B is full)
    expected = sorted(set(env.cmd("BITS.TOARRAY", src)) & set(env.cmd("BITS.TOARRAY", other)))
    env.assertEqual(env.cmd("BITS.TOARRAY", dest), expected)


def test_and_compaction_merge(env: Env):
    # If both sources have an implicitly-filled cluster B, AND should preserve that cluster as implicitly-filled
    a = "and_comp_a"
    b = "and_comp_b"
    dest = "and_comp_dest2"

    base_a = 1 * 256
    base_b = 3 * 256
    base_c = 5 * 256

    # prepare both sources with cluster B fully filled, with min/max outside so compaction can occur
    env.cmd("BITS.INSERT", a, base_a - 1)
    env.cmd("BITS.INSERT", a, base_a)
    env.cmd("BITS.INSERT", a, base_c - 1)
    env.cmd("BITS.INSERT", a, base_c)
    env.cmd("BITS.INSERT", a, base_b)
    env.cmd("BITS.INSERT", a, *list(range(base_b, base_b + 256))[1:])

    env.cmd("BITS.INSERT", b, base_a)
    env.cmd("BITS.INSERT", b, base_a + 1)
    env.cmd("BITS.INSERT", b, base_c)
    env.cmd("BITS.INSERT", b, base_c + 1)
    env.cmd("BITS.INSERT", b, base_b)
    env.cmd("BITS.INSERT", b, *list(range(base_b, base_b + 256))[1:])

    # AND them; result should keep cluster B implicitly-filled (no resident clusters)
    env.cmd("BITS.OP", "AND", dest, a, b)
    info = env.cmd("BITS.INFO", dest)
    info_map = dict(zip(info[::2], info[1::2]))
    env.assertEqual(info_map[b'total_clusters'], 0)
    env.assertEqual(env.cmd("BITS.COUNT", dest), 258)


def test_node16_or_compaction(env: Env):
    """Scenario: OR creating full subnodes (Compaction)"""
    # Create two sets that when ORed will result in a full cluster that should be compacted
    
    env.cmd("BITS.INSERT", "s1", 0, 1000) 
    # Fill first half of cluster 1 (256-383)
    env.cmd("BITS.INSERT", "s1", *range(256, 256 + 128))
    
    env.cmd("BITS.INSERT", "s2", 0, 1000)
    # Fill second half of cluster 1 (384-511)
    env.cmd("BITS.INSERT", "s2", *range(256 + 128, 256 + 256))
    
    env.cmd("BITS.OP", "OR", "dest", "s1", "s2")
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    
    # Cluster 1 is full, and we also have min=0 and max=1000.
    # Bits 0 and 1000 are min_/max_, cluster 1 contains [256, 511].
    # total_clusters counts resident clusters. Since cluster 1 is full, it's non-resident.
    env.assertEqual(info_map['total_clusters'], 0)
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), 256 + 2) 
    env.assertEqual(env.cmd("BITS.GET", "dest", 256 + 42), 1)


def test_node16_and_resident_from_nonresident(env: Env):
    """Scenario: AND resulting in resident subnode from non-resident (implicitly filled)"""
    # S1 has full cluster 1
    env.cmd("BITS.INSERT", "s1", 0, 1000)
    env.cmd("BITS.INSERT", "s1", *range(256, 256 + 256))
    
    # S2 has partial cluster 1
    env.cmd("BITS.INSERT", "s2", 0, 1000)
    env.cmd("BITS.INSERT", "s2", 256, 257, 258)
    
    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    
    # dest should have cluster 1 resident because it's partial (equal to s2's cluster 1)
    env.assertEqual(info_map['total_clusters'], 1)
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0, 256, 257, 258, 1000])


def test_node16_xor_resident_from_nonresident(env: Env):
    """Scenario: XOR resulting in resident subnode from non-resident"""
    # S1 has full cluster 1
    env.cmd("BITS.INSERT", "s1", 0, 1000)
    env.cmd("BITS.INSERT", "s1", *range(256, 256 + 256))
    
    # S2 has partial cluster 1 (first 10 bits)
    env.cmd("BITS.INSERT", "s2", 0, 1000)
    env.cmd("BITS.INSERT", "s2", *range(256, 256 + 10))
    
    env.cmd("BITS.OP", "XOR", "dest", "s1", "s2")
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    
    # Bits 0 and 1000 are in both, so they cancel out.
    # Result is range [256+10, 256+255].
    # Cluster 1 should be resident.
    env.assertEqual(info_map['total_clusters'], 1)
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), 256 - 10)


def test_node16_nonresident_before_minimal_resident(env: Env):
    """Scenario: nonresident subnodes before the minimal resident subnode"""
    # Full cluster at index 1, partial cluster at index 2.
    env.cmd("BITS.INSERT", "s1", 0, 2000)
    # Cluster 1 (256-511) full
    env.cmd("BITS.INSERT", "s1", *range(256, 256 + 256))
    # Cluster 2 (512-767) partial
    env.cmd("BITS.INSERT", "s1", 512, 513)
    
    info = env.cmd("BITS.INFO", "s1")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1) 
    
    # Successor from 1 should find 256 (from full cluster 1)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "s1", 1), 256)
    # Successor from 511 should find 512 (from resident cluster 2)
    env.assertEqual(env.cmd("BITS.SUCCESSOR", "s1", 511), 512)


def test_node16_min_promotion_from_nonresident(env: Env):
    """Scenario: AND where min is removed, and we promote from a non-resident (full) cluster"""
    # S1: [10, 256..511, 1000] (cluster 1 is full)
    # S2: [20, 256..511, 1000] (cluster 1 is full)
    # S1 AND S2: min 10 and 20 are different, so they are not in intersection.
    # New min should be 256.
    
    env.cmd("BITS.INSERT", "s1", 10, 1000)
    env.cmd("BITS.INSERT", "s1", *range(256, 256 + 256))
    
    env.cmd("BITS.INSERT", "s2", 20, 1000)
    env.cmd("BITS.INSERT", "s2", *range(256, 256 + 256))
    
    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")
    
    # Intersection should be [256..511, 1000]
    # New min_ is 256. New max_ is 1000.
    # Cluster 1 now contains [257..511], so it has 255 elements.
    # A cluster with 255 elements is RESIDENT.
    
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), list(range(256, 256 + 256)) + [1000])
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1)


def test_node16_max_promotion_from_nonresident(env: Env):
    """Scenario: AND where max is removed, and we promote from a non-resident (full) cluster"""
    # S1: [0, 256..511, 990]
    # S2: [0, 256..511, 980]
    # S1 AND S2: max 990 and 980 differ.
    # New max should be 511.
    
    env.cmd("BITS.INSERT", "s1", 0, 990)
    env.cmd("BITS.INSERT", "s1", *range(256, 256 + 256))
    
    env.cmd("BITS.INSERT", "s2", 0, 980)
    env.cmd("BITS.INSERT", "s2", *range(256, 256 + 256))
    
    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")
    
    # Intersection should be [0, 256..511]
    # New min_ is 0. New max_ is 511.
    # Cluster 1 now contains [256..510], so it has 255 elements.
    # A cluster with 255 elements is RESIDENT.
    
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0] + list(range(256, 256 + 256)))
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1)


def test_node16_complex_resident_nonresident_mix(env: Env):
    """Scenario: complex mix of resident and non-resident subnodes in set ops"""
    # S1: [0, C1(full), C2(resident: [512, 513]), 2000]
    env.cmd("BITS.INSERT", "s1", 0, 2000)
    env.cmd("BITS.INSERT", "s1", *range(256, 512))
    env.cmd("BITS.INSERT", "s1", 512, 513)
    
    # S2: [0, C1(resident: [256, 257]), C2(full), 2000]
    env.cmd("BITS.INSERT", "s2", 0, 2000)
    env.cmd("BITS.INSERT", "s2", 256, 257)
    env.cmd("BITS.INSERT", "s2", *range(512, 768))
    
    # OR: [0, C1(full), C2(full), 2000]
    env.cmd("BITS.OP", "OR", "res_or", "s1", "s2")
    env.assertEqual(env.cmd("BITS.COUNT", "res_or"), 256 + 256 + 2)
    info = env.cmd("BITS.INFO", "res_or")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 0) # both C1 and C2 are full
    
    # AND: [0, C1(resident: [256, 257]), C2(resident: [512, 513]), 2000]
    env.cmd("BITS.OP", "AND", "res_and", "s1", "s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "res_and"), [0, 256, 257, 512, 513, 2000])
    info = env.cmd("BITS.INFO", "res_and")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 2) # both resident
    
    # XOR: [C1(resident: 258..511), C2(resident: 514..767)]
    # 0 and 2000 cancel out.
    # C1: S1(full) XOR S2(256, 257) = 258..511 (resident)
    # C2: S1(512, 513) XOR S2(full) = 514..767 (resident)
    env.cmd("BITS.OP", "XOR", "res_xor", "s1", "s2")
    # C1 has 256 - 2 = 254 elements. C2 has 256 - 2 = 254 elements.
    env.assertEqual(env.cmd("BITS.COUNT", "res_xor"), 254 + 254)
    info = env.cmd("BITS.INFO", "res_xor")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 2)


def test_node16_or_desync_edge_case(env: Env):
    """Scenario: OR where a resident cluster becomes non-resident due to overlap with a full cluster,
    potentially desyncing subsequent resident clusters."""
    # S1: [0, C1(resident: [266]), C2(resident: [532]), 10000]
    # 266 = 256 + 10, 532 = 512 + 20
    env.cmd("BITS.INSERT", "s1", 0, 10000)
    env.cmd("BITS.INSERT", "s1", 266)
    env.cmd("BITS.INSERT", "s1", 532)
    
    # S2: [0, C1(full: [256..511]), C2(resident: [542]), 10000]
    # 542 = 512 + 30
    env.cmd("BITS.INSERT", "s2", 0, 10000)
    env.cmd("BITS.INSERT", "s2", *range(256, 512))
    env.cmd("BITS.INSERT", "s2", 542)
    
    # OR result:
    # C1: S1(resident: 266) OR S2(full) = Full (non-resident)
    # C2: S1(resident: 532) OR S2(resident: 542) = [532, 542] (resident)
    # Expected: [0, C1(full), C2([532, 542]), 10000]
    
    env.cmd("BITS.OP", "OR", "dest", "s1", "s2")
    
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), 256 + 2 + 2)
    env.assertEqual(env.cmd("BITS.GET", "dest", 532), 1) # This should fail if desynced (it would get bit 10 instead of 20)
    env.assertEqual(env.cmd("BITS.GET", "dest", 542), 1)
    env.assertEqual(env.cmd("BITS.GET", "dest", 266), 1)
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1)
    
    # Verify exact contents
    expected = sorted({0, 10000} | set(range(256, 512)) | {532, 542})
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), expected)


def test_node16_xor_full_resident_mix(env: Env):
    """Scenario: XOR where one side is full and the other is resident."""
    # S1: [0, C1(full: [256..511]), 1000]
    env.cmd("BITS.INSERT", "s1", 0, 1000)
    env.cmd("BITS.INSERT", "s1", *range(256, 512))
    
    # S2: [0, C1(resident: [256, 257, 258]), 1000]
    env.cmd("BITS.INSERT", "s2", 0, 1000)
    env.cmd("BITS.INSERT", "s2", 256, 257, 258)
    
    # XOR result:
    # 0 and 1000 cancel out.
    # C1: Full ^ [256, 257, 258] = [259..511]
    # Cluster 1 should be resident with 256 - 3 = 253 elements.
    
    env.cmd("BITS.OP", "XOR", "dest", "s1", "s2")
    
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), 253)
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1)
    
    # Check some values
    env.assertEqual(env.cmd("BITS.GET", "dest", 256), 0)
    env.assertEqual(env.cmd("BITS.GET", "dest", 259), 1)
    env.assertEqual(env.cmd("BITS.GET", "dest", 511), 1)


def test_node16_and_full_resident_mix(env: Env):
    """Scenario: AND where one side is full and the other is resident."""
    # S1: [0, C1(full: [256..511]), 1000]
    env.cmd("BITS.INSERT", "s1", 0, 1000)
    env.cmd("BITS.INSERT", "s1", *range(256, 512))
    
    # S2: [0, C1(resident: [256, 257, 258]), 1000]
    env.cmd("BITS.INSERT", "s2", 0, 1000)
    env.cmd("BITS.INSERT", "s2", 256, 257, 258)
    
    # AND result: [0, 256, 257, 258, 1000]
    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")
    
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0, 256, 257, 258, 1000])
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1)


def test_node16_or_two_full_clusters(env: Env):
    """Scenario: OR where both sides have a full cluster (should stay non-resident/full)."""
    env.cmd("BITS.INSERT", "s1", 0, 1000)
    env.cmd("BITS.INSERT", "s1", *range(256, 512))
    
    env.cmd("BITS.INSERT", "s2", 0, 1000)
    env.cmd("BITS.INSERT", "s2", *range(256, 512))
    
    env.cmd("BITS.OP", "OR", "dest", "s1", "s2")
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 0) # Still full/non-resident
    env.assertEqual(env.cmd("BITS.COUNT", "dest"), 256 + 2)


def test_node16_and_promotion_desync_edge_case(env: Env):
    """Scenario: AND where a resident cluster is removed, potentially desyncing subsequent clusters."""
    # S1: [0, C1(resident: [266]), C2(resident: [532]), 10000]
    env.cmd("BITS.INSERT", "s1", 0, 10000)
    env.cmd("BITS.INSERT", "s1", 266)
    env.cmd("BITS.INSERT", "s1", 532)
    
    # S2: [0, C2(resident: [532]), 10000]
    # C1 is empty in S2
    env.cmd("BITS.INSERT", "s2", 0, 10000)
    env.cmd("BITS.INSERT", "s2", 532)
    
    # AND result:
    # C1: S1(resident: 266) AND S2(empty) = Empty
    # C2: S1(resident: 532) AND S2(resident: 532) = [532]
    # Expected: [0, C2([532]), 10000]
    
    env.cmd("BITS.OP", "AND", "dest", "s1", "s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [0, 532, 10000])
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 1)


def test_node16_xor_promotion_desync_edge_case(env: Env):
    """Scenario: XOR where a resident cluster is removed or changed, potentially desyncing."""
    # S1: [0, C1(resident: [266]), C2(resident: [532]), 10000]
    env.cmd("BITS.INSERT", "s1", 0, 10000)
    env.cmd("BITS.INSERT", "s1", 266)
    env.cmd("BITS.INSERT", "s1", 532)
    
    # S2: [0, C1(resident: [266]), C2(resident: [542]), 10000]
    env.cmd("BITS.INSERT", "s2", 0, 10000)
    env.cmd("BITS.INSERT", "s2", 266)
    env.cmd("BITS.INSERT", "s2", 542)
    
    # XOR result:
    # 0 and 10000 cancel out
    # C1: [266] XOR [266] = Empty
    # C2: [532] XOR [542] = [532, 542]
    # Expected: [532, 542]
    
    env.cmd("BITS.OP", "XOR", "dest", "s1", "s2")
    env.assertEqual(env.cmd("BITS.TOARRAY", "dest"), [532, 542])
    
    info = env.cmd("BITS.INFO", "dest")
    info_map = {k.decode() if isinstance(k, bytes) else k: v for k, v in zip(info[::2], info[1::2])}
    env.assertEqual(info_map['total_clusters'], 0)
