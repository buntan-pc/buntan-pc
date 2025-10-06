#!/usr/bin/python3

class Variable:
    def __init__(self, name):
        self._name = name

    def __str__(self):
        return self._name

class Temporary:
    def __init__(self, name):
        self._name = name

    def __str__(self):
        return '_' + self._name

class Number:
    def __init__(self, num):
        self._num = num

    def __str__(self):
        return self._num

class Register:
    def __init__(self, num):
        self._num = num

    def __str__(self):
        return f'R{self._num}'

    @property
    def num(self):
        return self._num

class OneBlockCodeGenerator:
    '''
    This generates code for one basic block.
    '''
    def __init__(self, reg_num, var_names):
        '''
        reg_num: The number of registers
        var_names: Names of variables alive at the exit of this block
        '''
        self._rd = [set() for _ in range(reg_num)]  # rd[i]: set of var
        self._ad = dict()                           # ad[var]: set of var or reg
        self._var_names_alive_at_exit = var_names

    def gen_asm(self, tai):
        return tai.gen_asm(self._rd, self._ad)

def print_rd(rd):
    print('RD', end=' ')
    for i, vs in enumerate(rd):
        print(f'{i}:{",".join(str(v) for v in vs)}', end=' ')
    print()

def print_ad(ad):
    print('AD', end=' ')
    for var, addrs in ad.items():
        print(f'{var}:{",".join(str(a) for a in addrs)}', end=' ')
    print()

class TAI:
    def __init__(self):
        pass

def get_reg_having_var(var, ad):
    addrs = ad[var]
    regs = [a for a in addrs if isinstance(a, Register)]
    print(f'get_reg_having_var({var}, [{",".join(str(a) for a in addrs)}]) -> {regs}')
    return regs[0] if regs else None

def get_free_reg(rd):
    free_regs = [i for i in range(len(rd)) if len(rd[i]) == 0]
    return Register(free_regs[0]) if free_regs else None

# 指定したレジスタを空にするためにスピルが必要な変数のリストを返す
def get_spill_vars(reg, rd, ad):
    vs = rd[reg.num]
    spill = set()
    for v in vs:
        # reg_i 以外に v の最新値を保持している場所があるかを検査
        addrs = [a for a in ad[v] if a != reg]
        if len(addrs) == 0:
            spill.add(v)
    return spill

def get_reg_for_read(var, rd, ad):
    r = get_reg_having_var(var, ad)
    if r:
        return r, set()
    r = get_free_reg(rd)
    if r:
        return r, set()

    spill_vars_for_reg = []
    for i in range(len(rd)):
        reg = Register(i)
        spill_vars = get_spill_vars(reg, rd, ad)
        spill_vars_for_reg.append((reg, spill_vars))

    min_score_reg, min_score_spill_vars = sorted(spill_vars_for_reg, key=lambda elem: len(elem[1]))[0]
    return min_score_reg, min_score_spill_vars

def get_reg_for_write(var, rd, ad):
    r = get_reg_having_var(var, ad)
    if r and len(rd[r.num]) == 1:
        # r は var だけを持っている
        return r, set()
    r = get_free_reg(rd)
    if r:
        return r, set()

    spill_vars_for_reg = []
    for i in range(len(rd)):
        reg = Register(i)
        spill_vars = get_spill_vars(reg, rd, ad) - {var}
        spill_vars_for_reg.append((reg, spill_vars))

    min_score_reg, min_score_spill_vars = sorted(spill_vars_for_reg, key=lambda elem: len(elem[1]))[0]
    return min_score_reg, min_score_spill_vars

class BinOp(TAI):
    def __init__(self, dst, op, l, r):
        self._dst = dst
        self._op = op
        self._l = l
        self._r = r

    def __str__(self):
        return f'{self._op}  {self._dst}, {self._l}, {self._r}'

    @property
    def dst(self):
        return self._dst

    def gen_asm(self, rd, ad):
        asm = []

        if self._l not in ad:
            ad[self._l] = set()
            if isinstance(self._l, Variable):
                ad[self._l].add(self._l)

        l_reg, spill_vars = get_reg_for_read(self._l, rd, ad)
        for spill_var in spill_vars:
            asm.append(f'ST {spill_var}, {l_reg}')
        if self._l not in rd[l_reg.num]:
            rd[l_reg.num].add(self._l)
            ad[self._l].add(l_reg)
            asm.append(f'LD {l_reg}, {self._l}')

        print_ad(ad)

        if self._r not in ad:
            ad[self._r] = set()
            if isinstance(self._l, Variable):
                ad[self._r].add(self._r)

        r_reg, spill_vars = get_reg_for_read(self._r, rd, ad)
        for spill_var in spill_vars:
            asm.append(f'ST {spill_var}, {r_reg}')
        if self._r not in rd[r_reg.num]:
            rd[r_reg.num].add(self._r)
            ad[self._r].add(r_reg)
            asm.append(f'LD {r_reg}, {self._r}')

        print_ad(ad)

        if self._dst not in ad:
            ad[self._dst] = set()

        d_reg, spill_vars = get_reg_for_write(self._dst, rd, ad)
        for spill_var in spill_vars:
            asm.append(f'ST {spill_var}, {d_reg}')

        rd[d_reg.num].add(self._dst)
        ad[self._dst].add(d_reg)
        asm.append(f'{self._op} {d_reg}, {l_reg}, {r_reg}')

        return asm

class Copy(TAI):
    def __init__(self, dst, src):
        self._dst = dst
        self._src = src

    def __str__(self):
        return f'=  {self._dst}, {self._src}'

    @property
    def dst(self):
        return self._dst

    def gen_asm(self, rd, ad):
        pass

def var_parse(name):
    if name.startswith('_'):
        return Temporary(name[1:])
    elif name.isdigit():
        return Number(int(name))
    else:
        return Variable(name)

def tai_parse(src):
    elems = src.split()
    dst = var_parse(elems.pop(0))
    op = elems.pop(0)

    if op != '=':
        raise ValueError('not implemented for instructions other than a normal assignment')

    lhs = var_parse(elems.pop(0))

    if len(elems) == 2:
        binop = elems.pop(0)
        rhs = var_parse(elems.pop(0))
        return BinOp(dst, binop, lhs, rhs)
    elif len(elems) == 0:
        return Copy(dst, lhs)

def tac_parse(src):
    '''
    Parse given three address codes, generate an array of TAI object.
    '''
    tac = []
    for line in src.splitlines():
        tai = tai_parse(line.strip())
        tac.append(tai)
    return tac

if __name__ == '__main__':
    three_addr_code = tac_parse('''\
_0 = a - b
_1 = a - c
_2 = _0 + _1
a = d
d = _2 + _1
''')
    gen = OneBlockCodeGenerator(4, {'a', 'b', 'c', 'd'})
    for tai in three_addr_code:
        print(tai)
        print(gen.gen_asm(tai))
        print_rd(gen._rd)
        print_ad(gen._ad)
