from RLTest import Env


def test_invalid_negative(env: Env):
    env.expect('bits.insert', 'inv', -1).error().contains('invalid element')


def test_wrong_type(env: Env):
    env.cmd('SET', 'strkey', 'value')
    env.expect('bits.insert', 'strkey', 1).error().contains('WRONGTYPE') 