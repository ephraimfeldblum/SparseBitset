from RLTest import Env


def test_invalid_negative(env: Env):
    env.expect('BITS.INSERT', 'inv', -1).error().contains('invalid element')


def test_wrong_type(env: Env):
    env.cmd('SET', 'strkey', 'value')
    env.expect('BITS.INSERT', 'strkey', 1).error().contains('WRONGTYPE') 