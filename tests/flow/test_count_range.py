import random
import pytest


def add(env, key, v):
    return env.cmd('BITS.INSERT', key, v)

def remove(env, key, v):
    return env.cmd('BITS.REMOVE', key, v)

def count_range(env, key, lo, hi):
    return int(env.cmd('BITS.COUNT', key, lo, hi, 'BIT'))

def ref_count(s, lo, hi):
    return sum(1 for x in s if lo <= x <= hi)

def reset(env):
    env.cmd('FLUSHALL')

def test_count_range_basic(env):
    reset(env)
    key = "veb1"
    s = set()
    for v in range(0, 200):
        add(env, key, v)
        s.add(v)
    env.assertEqual(count_range(env, key, 10, 20), ref_count(s, 10, 20))
    env.assertEqual(count_range(env, key, 0, 0), ref_count(s, 0, 0))
    env.assertEqual(count_range(env, key, 199, 300), ref_count(s, 199, 300))
    env.assertEqual(count_range(env, key, -100, 1000), ref_count(s, -100, 1000))

def test_count_range_promote_min_on_delete(env):
    reset(env)
    key = "veb_promote_min"
    s = set([1,2,3,1000])
    for v in s:
        add(env, key, v)
    # remove current min (1) so min should promote to 2 internally
    remove(env, key, 1)
    s.remove(1)
    env.assertEqual(count_range(env, key, 1, 1), ref_count(s, 1, 1))
    env.assertEqual(count_range(env, key, 2, 3), ref_count(s, 2, 3))
    # remove more to force promotion from cluster (remove 2 -> min becomes 3)
    remove(env, key, 2)
    s.remove(2)
    env.assertEqual(count_range(env, key, 2, 1000), ref_count(s, 2, 1000))
    env.assertEqual(count_range(env, key, 3, 1000), ref_count(s, 3, 1000))
def test_count_range_promote_max_on_delete(env):
    reset(env)
    key = "veb_promote_max"
    s = set([0,1,2,3,4])
    for v in s:
        add(env, key, v)
    # delete current max, expect max to move down
    remove(env, key, 4)
    s.remove(4)
    env.assertEqual(count_range(env, key, 3, 4), ref_count(s, 3, 4))
    remove(env, key, 3)
    s.remove(3)
    env.assertEqual(count_range(env, key, 0, 10), ref_count(s, 0, 10))

def test_count_range_cross_cluster_boundaries(env):
    reset(env)
    key = "veb_clusters"
    # pick positions that are likely to span different clusters/levels:
    pts = [0, 15, 16, 17, 255, 256, 257, 1023, 1024, 1025]
    s = set(pts)
    for v in pts:
        add(env, key, v)
    # queries across boundaries
    env.assertEqual(count_range(env, key, 15, 16), ref_count(s, 15, 16))
    env.assertEqual(count_range(env, key, 16, 256), ref_count(s, 16, 256))
    env.assertEqual(count_range(env, key, 0, 1025), ref_count(s, 0, 1025))
    # remove some boundary elements and ensure counts update correctly
    remove(env, key, 16)
    s.remove(16)
    remove(env, key, 256)
    s.remove(256)
    env.assertEqual(count_range(env, key, 15, 256), ref_count(s, 15, 256))
    env.assertEqual(count_range(env, key, 256, 1025), ref_count(s, 256, 1025))

def test_count_range_randomized_small(env):
    reset(env)
    key = "veb_rand"
    s = set()
    N = 300
    # perform a sequence of inserts/removes
    for _ in range(N):
        v = random.randint(0, 2000)
        if random.random() < 0.6:
            add(env, key, v)
            s.add(v)
        else:
            remove(env, key, v)
            s.discard(v)
    # run many random range checks and compare with reference
    for _ in range(200):
        a = random.randint(0, 2000)
        b = random.randint(0, 2000)
        lo, hi = min(a, b), max(a, b)
        env.assertEqual(count_range(env, key, lo, hi), ref_count(s, lo, hi))
