import os
import random
from RLTest import Env

def test_fuzz_basic_ops(env: Env):
    """Randomized testing for basic bitset operations"""
    seed = int(os.environ.get('FUZZ_SEED', '42'))
    num_iterations = int(os.environ.get('FUZZ_ITER', '200'))
    random.seed(seed)
    ref_sets = {}
    key_prefix = "fuzz_set_"
    num_sets = 5
    max_val = 1000

    def get_ref(key):
        if key not in ref_sets:
            ref_sets[key] = set()
        return ref_sets[key]

    for i in range(num_iterations):
        op = random.choice(["INSERT", "REMOVE", "GET", "COUNT", "MIN", "MAX", "SUCCESSOR", "PREDECESSOR"])
        key_idx = random.randint(0, num_sets - 1)
        key = f"{key_prefix}{key_idx}"
        ref = get_ref(key)

        if op == "INSERT":
            vals = [random.randint(0, max_val) for _ in range(random.randint(1, 5))]
            added = 0
            for v in set(vals):
                if v not in ref:
                    ref.add(v)
                    added += 1
            env.assertEqual(env.cmd("BITS.INSERT", key, *vals), added)
        
        elif op == "REMOVE":
            vals = [random.randint(0, max_val) for _ in range(random.randint(1, 5))]
            removed = 0
            for v in set(vals):
                if v in ref:
                    ref.remove(v)
                    removed += 1
            env.assertEqual(env.cmd("BITS.REMOVE", key, *vals), removed)
        
        elif op == "GET":
            val = random.randint(0, max_val)
            env.assertEqual(env.cmd("BITS.GET", key, val), 1 if val in ref else 0)
        
        elif op == "COUNT":
            env.assertEqual(env.cmd("BITS.COUNT", key), len(ref))
        
        elif op == "MIN":
            res = env.cmd("BITS.MIN", key)
            if not ref:
                env.assertEqual(res, None)
            else:
                env.assertEqual(res, min(ref))
        
        elif op == "MAX":
            res = env.cmd("BITS.MAX", key)
            if not ref:
                env.assertEqual(res, None)
            else:
                env.assertEqual(res, max(ref))
        
        elif op == "SUCCESSOR":
            val = random.randint(0, max_val)
            res = env.cmd("BITS.SUCCESSOR", key, val)
            succs = [v for v in ref if v > val]
            if not succs:
                env.assertEqual(res, None)
            else:
                env.assertEqual(res, min(succs))
        
        elif op == "PREDECESSOR":
            val = random.randint(0, max_val)
            res = env.cmd("BITS.PREDECESSOR", key, val)
            preds = [v for v in ref if v < val]
            if not preds:
                env.assertEqual(res, None)
            else:
                env.assertEqual(res, max(preds))

def test_fuzz_set_ops(env: Env):
    """Randomized testing for set operations (AND, OR, XOR)"""
    seed = int(os.environ.get('FUZZ_SEED', '42'))
    num_iterations = int(os.environ.get('FUZZ_ITER', '200'))
    random.seed(seed)
    ref_sets = {}
    key_prefix = "fuzz_op_set_"
    num_sets = 5
    max_val = 500

    for i in range(num_sets):
        key = f"{key_prefix}{i}"
        ref_sets[key] = set(random.sample(range(max_val), random.randint(10, 50)))
        env.cmd("BITS.INSERT", key, *list(ref_sets[key]))

    for i in range(num_iterations):
        op_type = random.choice(["AND", "OR", "XOR"])
        dest_idx = random.randint(0, num_sets + 2) # some new keys
        dest_key = f"{key_prefix}dest_{dest_idx}"
        
        num_sources = random.randint(1, num_sets)
        source_indices = random.sample(range(num_sets), num_sources)
        source_keys = [f"{key_prefix}{idx}" for idx in source_indices]
        
        res = env.cmd("BITS.OP", op_type, dest_key, *source_keys)
        
        # Calculate expected result
        expected = ref_sets[source_keys[0]].copy()
        for j in range(1, len(source_keys)):
            s = ref_sets[source_keys[j]]
            if op_type == "AND":
                expected &= s
            elif op_type == "OR":
                expected |= s
            elif op_type == "XOR":
                expected ^= s
        
        ref_sets[dest_key] = expected
        
        # Check return value (byte size)
        if not expected:
            env.assertEqual(res, 0)
        else:
            env.assertEqual(res, (max(expected) // 8) + 1)
            
        env.assertEqual(env.cmd("BITS.COUNT", dest_key), len(expected))
        if len(expected) < 100: # Don't check TOARRAY for very large sets to keep it fast
            env.assertEqual(sorted(list(expected)), env.cmd("BITS.TOARRAY", dest_key))


def test_fuzz_extended(env: Env):
    """Extended reproducible fuzz test. Controlled by FUZZ_ITER and FUZZ_SEED env vars.

    Keep default iterations small to avoid long CI runs; can be increased locally.
    """
    seed = int(os.environ.get('FUZZ_SEED', '42'))
    num_iterations = int(os.environ.get('FUZZ_ITER', '200'))
    random.seed(seed)

    ref = set()
    key = 'fuzz_ext'

    for i in range(num_iterations):
        op = random.choice(['INS', 'REM', 'GET', 'COUNT'])
        if op == 'INS':
            vals = [random.randint(0, 2000) for _ in range(random.randint(1, 10))]
            added = 0
            for v in set(vals):
                if v not in ref:
                    ref.add(v)
                    added += 1
            env.assertEqual(env.cmd('BITS.INSERT', key, *vals), added)
        elif op == 'REM':
            vals = [random.randint(0, 2000) for _ in range(random.randint(1, 10))]
            removed = 0
            for v in set(vals):
                if v in ref:
                    ref.remove(v)
                    removed += 1
            env.assertEqual(env.cmd('BITS.REMOVE', key, *vals), removed)
        elif op == 'GET':
            v = random.randint(0, 2000)
            env.assertEqual(env.cmd('BITS.GET', key, v), 1 if v in ref else 0)
        elif op == 'COUNT':
            env.assertEqual(env.cmd('BITS.COUNT', key), len(ref))

    # final sanity checks
    env.assertEqual(env.cmd('BITS.COUNT', key), len(ref))
    if len(ref) < 1000:
        env.assertEqual(sorted(list(ref)), env.cmd('BITS.TOARRAY', key))
