from RLTest import Env


def test_invalid_negative(env: Env):
    env.expect('BITS.INSERT', 'inv', -1).error().contains('invalid element')
    env.expect('BITS.REMOVE', 'inv', -1).error().contains('invalid element')
    env.expect('BITS.GET', 'inv', -1).error().contains('out of range')
    env.expect('BITS.SET', 'inv', -1, 1).error().contains('out of range')
    env.expect('BITS.SUCCESSOR', 'inv', -1).error().contains('invalid element')
    env.expect('BITS.PREDECESSOR', 'inv', -1).error().contains('invalid element')


def test_wrong_type(env: Env):
    env.cmd('SET', 'strkey', 'value')
    env.expect('BITS.INSERT', 'strkey', 1).error().contains('WRONGTYPE')
    env.expect('BITS.GET', 'strkey', 1).error().contains('WRONGTYPE')
    env.expect('BITS.SET', 'strkey', 1, 1).error().contains('WRONGTYPE')
    env.expect('BITS.REMOVE', 'strkey', 1).error().contains('WRONGTYPE')
    env.expect('BITS.COUNT', 'strkey').error().contains('WRONGTYPE')
    env.expect('BITS.CLEAR', 'strkey').error().contains('WRONGTYPE')
    env.expect('BITS.MIN', 'strkey').error().contains('WRONGTYPE')
    env.expect('BITS.MAX', 'strkey').error().contains('WRONGTYPE')
    env.expect('BITS.SUCCESSOR', 'strkey', 1).error().contains('WRONGTYPE')
    env.expect('BITS.PREDECESSOR', 'strkey', 1).error().contains('WRONGTYPE')
    env.expect('BITS.TOARRAY', 'strkey').error().contains('WRONGTYPE')
    env.expect('BITS.POS', 'strkey', 1).error().contains('WRONGTYPE')
    env.expect('BITS.INFO', 'strkey').error().contains('WRONGTYPE')


def test_invalid_bits_set_value(env: Env):
    env.expect('BITS.SET', 'bs', 1, 2).error().contains('must be 0 or 1')
    env.expect('BITS.SET', 'bs', 1, -1).error().contains('must be 0 or 1')


def test_invalid_count_syntax(env: Env):
    env.cmd('BITS.INSERT', 'bs', 1)
    env.expect('BITS.COUNT', 'bs', 0, 10, 'INVALID').error().contains('syntax error')


def test_invalid_pos_syntax(env: Env):
    env.cmd('BITS.INSERT', 'bs', 1)
    env.expect('BITS.POS', 'bs', 2, 0, 10).error().contains('must be 0 or 1')
    env.expect('BITS.POS', 'bs', 1, 0, 10, 'INVALID').error().contains('syntax error')
