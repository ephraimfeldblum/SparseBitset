from RLTest import Env
import time

def test_persistence_after_restart(env: Env):
    # Works for both RDB and AOF depending on RLTest flags
    # Insert many elements
    elements = [i for i in range(20000)]
    env.cmd("BITS.INSERT", "persist", *elements)
    env.dumpAndReload()  # restart with persistence mechanism configured
    env.assertEqual(env.cmd("BITS.GET", "persist", 42), 1)


def test_persistence_monostate(env: Env):
    # create an empty bitset handle (monostate) by creating then clearing
    env.cmd("BITS.CLEAR", "empty_monostate")
    env.dumpAndReload()
    # empty bitset should respond 0 for any bit
    env.assertEqual(env.cmd("BITS.GET", "empty_monostate", 0), 0)


def test_persistence_node8_roundtrip(env: Env):
    # insert values that fit into Node8 (0..255)
    values = [1, 42, 200]
    env.cmd("BITS.INSERT", "small", *values)
    env.dumpAndReload()
    # the inserted bits must survive reload
    for v in values:
        env.assertEqual(env.cmd("BITS.GET", "small", v), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "small"), len(values))


def test_persistence_node16_roundtrip(env: Env):
    # create a Node16 by inserting a value >255 first, then a smaller, then a middle value
    values = [300, 100, 200]
    env.cmd("BITS.CLEAR", "node16_test")
    env.cmd("BITS.INSERT", "node16_test", *values)
    env.dumpAndReload()
    for v in values:
        env.assertEqual(env.cmd("BITS.GET", "node16_test", v), 1)
    env.assertEqual(env.cmd("BITS.COUNT", "node16_test"), len(values))


def test_replica_sync(env: Env):
    if not getattr(env.envRunner, "useSlaves", False):
        env.skip()

    # write on master
    veb_replica = [i for i in range(5000)]
    env.cmd("BITS.INSERT", "repl", *veb_replica)

    # ask the master to wait until 1 replica has the write (5-second timeout)
    env.cmd("WAIT", 1, 10000)

    # now check on the replica
    r = env.getSlaveConnection()
    env.assertEqual(r.execute_command("BITS.COUNT", "repl"), 5000)