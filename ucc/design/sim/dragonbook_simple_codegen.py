#!/usr/bin/python3

from collections import OrderedDict

from asm import *
from parse import parse, NAME, NUMBER, OP

class Value:
    def __init__(self, prefix, value):
        self._prefix = prefix
        self._value = value

    def __str__(self):
        return f'{self._prefix}{self._value}'

    def __eq__(self, other):
        return self.__class__ == other.__class__ and self._value == other._value

    def __hash__(self):
        return hash(f'{self.__class__}{self._value}')

    @property
    def prefix(self):
        return self._prefix

    @property
    def value(self):
        return self._value

class Variable(Value):
    def __init__(self, name):
        super().__init__('', name)

    @property
    def name(self):
        return self.value

class Temporary(Value):
    def __init__(self, name):
        super().__init__('_', name)

    @property
    def name(self):
        return self.value

class Number(Value):
    def __init__(self, num):
        super().__init__('', num)

    @property
    def num(self):
        return self.value

class Register(Value):
    def __init__(self, num):
        super().__init__('R', num)

    @property
    def num(self):
        return self.value

def list_append_if_none(ls, elem):
    if elem not in ls:
        ls.append(elem)
    return ls

def list_remove_if_exist(ls, elem):
    if elem in ls:
        ls.remove(elem)
    return ls

class RegisterDescriptor:
    '''
    レジスタディスクリプタの表の整合性を保ちつつ更新するためのメソッドを持つ。
    整合性を保つことに集中し、それ以外のロジックは含めない。
    '''
    def __init__(self, reg_num):
        self._rd = [[] for _ in range(reg_num)]

    def __str__(self):
        return ' '.join(f'{i}:{",".join(str(v) for v in vs)}' for i, vs in enumerate(self._rd))

    def __getitem__(self, i):
        vs = self._rd[i.num if isinstance(i, Register) else i]
        return vs.copy()  # shallow copy

    def __len__(self):
        return len(self._rd)

    def assign_reg_for_read(self, reg, val):
        if val in self._rd[reg.num]:
            return False

        if isinstance(val, list):
            self._rd[reg.num] = val
        else:
            self._rd[reg.num] = [val]
        return True

    def assign_reg_for_write(self, reg, val):
        for i in range(len(self._rd)):
            if i == reg.num:
                if isinstance(val, list):
                    self._rd[i] = val
                else:
                    self._rd[i] = [val]
            else:
                list_remove_if_exist(self._rd[i], val)

    def get_reg_having_val_or_free(self, val):
        having_val = [i for i in range(len(self._rd)) if val in self._rd[i]]
        if having_val:
            return Register(having_val[0])
        free_regs = [i for i in range(len(self._rd)) if len(self._rd[i]) == 0]
        if free_regs:
            return Register(free_regs[0])
        return None

class AddressDescriptor:
    '''
    アドレスディスクリプタの表の整合性を保ちつつ更新するためのメソッドを持つ。
    整合性を保つことに集中し、それ以外のロジックは含めない。
    '''
    def __init__(self):
        self._ad = OrderedDict()

    def __str__(self):
        return ' '.join(f'{v}:{",".join(str(a) for a in addrs)}' for v, addrs in self._ad.items())

    def __getitem__(self, val):
        self._init_key(val)
        return self._ad[val]

    def _init_key(self, val):
        if val not in self._ad:
            self._ad[val] = []
            if isinstance(val, Variable):
                list_append_if_none(self._ad[val], val)

    def assign_reg_for_read(self, val, reg):
        self._init_key(val)
        for v in self._ad:
            if v == val:
                list_append_if_none(self._ad[v], reg)
            else:
                list_remove_if_exist(self._ad[v], reg)

    def assign_reg_for_write(self, val, reg, exclude=None):
        self._init_key(val)
        for v in self._ad:
            if v == val:
                self._ad[v] = [reg]
            elif v != exclude:
                list_remove_if_exist(self._ad[v], reg)

    def spill(self, val):
        self._ad[val] = [val]

    def get_vars_only_on_reg(self):
        vs = []
        for v, addrs in self._ad.items():
            if isinstance(v, Variable) and len(addrs) == 1:
                a = list(addrs)[0]
                if isinstance(a, Register):
                    vs.append((v, a))
        return vs

class OneBlockCodeGenerator:
    '''
    This generates code for one basic block.
    '''
    def __init__(self, reg_num, var_names):
        '''
        reg_num: The number of registers
        var_names: Names of variables alive at the exit of this block
        '''
        self._rd = RegisterDescriptor(reg_num)
        self._ad = AddressDescriptor()
        self._var_names_alive_at_exit = var_names

    def gen_asm(self, tai):
        return tai.gen_asm(self._rd, self._ad)

    def gen_st_vars(self):
        vs = self._ad.get_vars_only_on_reg()
        return [ST(v[0], v[1]) for v in vs]

def print_rd(rd):
    print('RD', rd)

def print_ad(ad):
    print('AD', ad)

class TAI:
    def __init__(self):
        pass

# 指定したレジスタを空にするためにスピルが必要な変数のリストを返す
def get_spill_vars(reg, rd, ad):
    vs = rd[reg.num]
    spill = []
    for v in vs:
        # reg_i 以外に v の最新値を保持している場所があるかを検査
        addrs = [a for a in ad[v] if a != reg]
        if len(addrs) == 0:
            spill.append(v)
    return spill

def get_reg_for_read(var, rd, ad, exclude):
    r = rd.get_reg_having_val_or_free(var)
    if r:
        return r, []

    spill_vars_for_reg = []
    for i in range(len(rd)):
        if i in exclude:
            continue
        reg = Register(i)
        spill_vars = get_spill_vars(reg, rd, ad)
        spill_vars_for_reg.append((reg, spill_vars))

    min_score_reg, min_score_spill_vars = sorted(spill_vars_for_reg, key=lambda elem: len(elem[1]))[0]
    return min_score_reg, min_score_spill_vars

def get_reg_for_write(var, rd, ad, exclude):
    r = rd.get_reg_having_val_or_free(var)
    if r and (rd[r] == [var] or len(rd[r]) == 0):
        # r は var だけを持っているか、空っぽ
        return r, []

    spill_vars_for_reg = []
    for i in range(len(rd)):
        if i in exclude:
            continue
        reg = Register(i)
        spill_vars = list_remove_if_exist(get_spill_vars(reg, rd, ad), var)
        spill_vars_for_reg.append((reg, spill_vars))

    min_score_reg, min_score_spill_vars = sorted(spill_vars_for_reg, key=lambda elem: len(elem[1]))[0]
    return min_score_reg, min_score_spill_vars

def gen_asm_reg_for_read(var, rd, ad, exclude=[]):
    asm = []

    reg, spill_vars = get_reg_for_read(var, rd, ad, exclude)
    for spill_var in spill_vars:
        asm.append(ST(spill_var, reg))
        ad.spill(spill_var)
    if rd.assign_reg_for_read(reg, var):
        asm.append(LD(reg, var))
    ad.assign_reg_for_read(var, reg)

    return reg, asm

class BinOp(TAI):
    def __init__(self, dst, op, l, r):
        self._dst = dst
        self._op = op
        self._l = l
        self._r = r

    def __str__(self):
        return f'{self._op}  {self._dst}, {self._l}, {self._r}'

    @property
    def values(self):
        return {self._dst, self._l, self._r}

    @property
    def dst(self):
        return self._dst

    def gen_asm(self, rd, ad):
        asm = []

        l_reg, l_asm = gen_asm_reg_for_read(self._l, rd, ad)
        asm.extend(l_asm)
        r_reg, r_asm = gen_asm_reg_for_read(self._r, rd, ad, exclude=[l_reg.num])
        asm.extend(r_asm)

        d_reg, spill_vars = get_reg_for_write(self._dst, rd, ad, exclude=[l_reg.num, r_reg.num])
        for spill_var in spill_vars:
            asm.append(ST(spill_var, d_reg))

        rd.assign_reg_for_write(d_reg, self._dst)
        ad.assign_reg_for_write(self._dst, d_reg)
        asm.append(get_op(self._op)(d_reg, l_reg, r_reg))

        return asm

class Copy(TAI):
    def __init__(self, dst, src):
        self._dst = dst
        self._src = src

    def __str__(self):
        return f'=  {self._dst}, {self._src}'

    @property
    def values(self):
        return {self._dst, self._src}

    @property
    def dst(self):
        return self._dst

    def gen_asm(self, rd, ad):
        reg, asm = gen_asm_reg_for_read(self._src, rd, ad)
        rd.assign_reg_for_write(reg, [self._src, self._dst])
        ad.assign_reg_for_read(self._src, reg)
        ad.assign_reg_for_write(self._dst, reg, exclude=self._src)
        return asm

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
    values = set()
    for line in src.splitlines():
        tai = tai_parse(line.strip())
        tac.append(tai)
        values.update(tai.values)
    return tac, values

tmp_cnt = 0
def gen_tmp():
    global tmp_cnt
    tmp_cnt += 1
    return Temporary(str(tmp_cnt))

def gen_tac_expr(expr):
    if len(expr.children) == 2:
        lhs, rhs = expr.children
        if expr.value == '=':
            dst = Variable(lhs.value)
            r_tac, r_var = gen_tac_expr(rhs)
            return r_tac + [Copy(dst, r_var)], dst
        else:
            l_tac, l_var = gen_tac_expr(lhs)
            r_tac, r_var = gen_tac_expr(rhs)
            dst = gen_tmp()
            return l_tac + r_tac + [BinOp(dst, expr.value, l_var, r_var)], dst
    else:
        if expr.type == NAME:
            return [], Variable(expr.value)
        elif expr.type == NUMBER:
            return [], Number(expr.value)
        raise ValueError(f'unexpected node: {expr}')

def gen_tac(prog):
    '''
    Generate an array of TAI object from the given program.
    '''
    tac = []
    values = set()
    for expr in prog:
        tac_for_expr, var = gen_tac_expr(expr)
        tac.extend(tac_for_expr)
        for tai in tac_for_expr:
            values.update(tai.values)
    return tac, values

def fill_str(s, n):
    l = len(s)
    if l >= n:
        return s
    n_sp = n - l
    return ' '*(n_sp//2) + s + ' '*((n_sp + 1)//2)

class InfoTablePrinter:
    def __init__(self, reg_num, variables, temporaries, tai_width=20, asm_width=20):
        self._reg_num = reg_num
        self._variables = variables
        self._temporaries = temporaries
        self._tai_width = tai_width
        self._asm_width = asm_width

    def print_header(self):
        print(' '*self._tai_width, end='|')
        print(' '*self._asm_width, end='|')
        print('|'.join(fill_str(str(Register(i)), 5) for i in range(self._reg_num)), end='|')
        print('|'.join(fill_str(str(v), 5) for v in self._variables), end='|')
        print('|'.join(fill_str(str(t), 5) for t in self._temporaries), end='|')
        print()

    def print_line(self, rd, ad, tai, insn):
        tai_str = '' if tai is None else str(tai)
        insn_str = '' if insn is None else str(insn)
        if tai is None and insn is None:
            tai_str = 'initial'

        print(tai_str.ljust(20), end='|')
        print(insn_str.ljust(20), end='|')
        for i in range(self._reg_num):
            print(f'{",".join(str(v) for v in rd[i]).ljust(5)}', end='|')
        for v in self._variables:
            print(f'{",".join(str(a) for a in ad[v]).ljust(5)}', end='|')
        for t in self._temporaries:
            print(f'{",".join(str(a) for a in ad[t]).ljust(5)}', end='|')
        print()

if __name__ == '__main__':
    give_tac_directly = True
    if give_tac_directly:
        src = '''\
_1 = a - b
_2 = a - c
_3 = _1 + _2
t = _3 + _2
a = d
d = t
'''
        three_addr_code, values = tac_parse(src)
    else:
        src = '''\
t = (a - b) + (a - c) + (a - c)
a = d
d = t
'''
        print('src:')
        print(src)
        prog = parse(src)
        three_addr_code, values = gen_tac(prog)

    print('\n'.join(str(tai) for tai in three_addr_code))

    variables = sorted((v for v in values if isinstance(v, Variable)), key=lambda v: v.name)
    temporaries = sorted((v for v in values if isinstance(v, Temporary)), key=lambda v: v.name)
    reg_num = 4

    info_printer = InfoTablePrinter(reg_num, variables, temporaries)
    info_printer.print_header()

    gen = OneBlockCodeGenerator(reg_num, {'a', 'b', 'c', 'd'})
    info_printer.print_line(gen._rd, gen._ad, None, None)

    for tai in three_addr_code:
        asm = gen.gen_asm(tai)
        if len(asm) > 0:
            for insn in asm[:-1]:
                info_printer.print_line(gen._rd, gen._ad, None, insn)
            insn = asm[-1]
            info_printer.print_line(gen._rd, gen._ad, tai, insn)
        else:
            info_printer.print_line(gen._rd, gen._ad, tai, None)

    for insn in gen.gen_st_vars():
        info_printer.print_line(gen._rd, gen._ad, None, insn)
