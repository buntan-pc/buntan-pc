#!/usr/bin/python3

from collections import namedtuple

class OneBlockCodeGenerator:
    '''
    This generates code for one basic block.
    '''
    def __init__(self, reg_num, var_names):
        '''
        reg_num: The number of registers
        var_names: Names of variables alive at the exit of this block
        '''
        pass

class TAI:
    def __init__(self):
        pass

class BinOp(TAI):
    def __init__(self, dst, l, r):
        self._dst = dst
        self._l = l
        self._r = r

    def gen_asm(self, rd, ad):
        pass

def tai_parse(src):
    eq_pos = src.find('=')
    if eq_pos < 0:
        raise ValueError('not implemented for non assignment instruction')

    lhs = src[0:eq_pos].strip()
    rhs = src[eq_pos+1:].strip()

    br_pos = lhs.find('[')
    if br_pos < 0:
        dst = lhs
    else:
        raise ValueError('not implemented for array indexing')

    br_pos = rhs.find('[')
    if br_pos < 0:
        i = 0
        while rhs[i].


def tac_parse(src):
    '''
    Parse given three address codes, generate an array of TAI object.
    '''
    tac = []
    for line in src.splitlines():
        tai = tai_parse(line.strip())
        tac.append(tai)
    tac

if __name__ == '__main__':
    three_addr_code = tac_parse('''\
_0 = a - b
_1 = a - c
_2 = _0 + _1
a = d
d = _2 + _1
''')
    gen = OneBlockCodeGenerator(4, {'a', 'b', 'c'})
    gen.gen_code()
