from RLTest import Env
import time

def test_persistence_after_restart(env: Env):
    # Works for both RDB and AOF depending on RLTest flags
    # Insert many elements
    elements = [i for i in range(20000)]
    env.cmd("BITS.INSERT", "persist", *elements)
    env.dumpAndReload()  # restart with persistence mechanism configured
    env.assertEqual(env.cmd("BITS.GET", "persist", 42), 1)


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