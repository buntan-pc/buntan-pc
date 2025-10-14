#!/usr/bin/python3

from collections import OrderedDict, defaultdict

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
    def __init__(self, reg_num, vars_alive_at_exit, def_use):
        '''
        reg_num: The number of registers
        var_names: Names of variables alive at the exit of this block
        '''
        self._rd = RegisterDescriptor(reg_num)
        self._ad = AddressDescriptor()
        self._vars_alive_at_exit = vars_alive_at_exit
        self._def_use = def_use

    def gen_asm(self, tai):
        return tai.gen_asm(self._rd, self._ad, self._def_use)

    def gen_st_vars(self):
        vs = self._ad.get_vars_only_on_reg()
        return [ST(v[0], v[1]) for v in vs if v[0] in self._vars_alive_at_exit]

def print_rd(rd):
    print('RD', rd)

def print_ad(ad):
    print('AD', ad)

class TAI:
    def __init__(self, line_num):
        self._line_num = line_num

    @property
    def line_num(self):
        return self._line_num

# 指定したレジスタを空にするためにスピルが必要な変数のリストを返す
def get_spill_vars(reg, rd, ad, def_use, line_num):
    vs = rd[reg.num]
    spill = []
    for v in [v for v in vs if not isinstance(v, Number)]:
        # reg 以外に v の最新値を保持している場所があるかを検査
        addrs = [a for a in ad[v] if a != reg]
        if len(addrs) == 0:
            # この変数が後に利用されることを確認
            for start, end in def_use[v]:
                if start > line_num:
                    break
                if start <= line_num < end:
                    spill.append(v)
                    break
    return spill

def get_reg_for_read(var, rd, ad, def_use, line_num, exclude):
    r = rd.get_reg_having_val_or_free(var)
    if r:
        return r, []

    spill_vars_for_reg = dict()
    for i in range(len(rd)):
        if i in exclude:
            continue
        reg = Register(i)
        spill_vars = get_spill_vars(reg, rd, ad, def_use, line_num)
        spill_vars_for_reg[reg] = spill_vars

    spill_score = sorted(((reg, len(vs)) for reg, vs in spill_vars_for_reg.items()), key=lambda item: item[1])
    min_score = spill_score[0][1]
    min_score_last_i = len(spill_score)
    for i in range(len(spill_score)):
        if spill_score[i][1] > min_score:
            min_score_last_i = i
            break
    if min_score_last_i == 1:
        min_score_reg = spill_score[0][0]
        return min_score_reg, spill_vars_for_reg[min_score_reg]

    # [0, min_score_last_i) の範囲はいずれもスピルコストが最小
    # 可能であれば、今後使わない変数をスピルさせたい
    use_after_for_reg = dict()
    for reg, _ in spill_score[0:min_score_last_i]:
        # この時点での tai の行数が欲しい
        use_after = 0
        for var in spill_vars_for_reg[reg]:
            for start, end in def_use[var]:
                if line_num < start:
                    break
                if start <= line_num < end:
                    # この変数の値は後に使われる
                    use_after += 1
                    break
        use_after_for_reg[reg] = use_after

    min_score_reg = sorted(use_after_for_reg.items(), key=lambda item: item[1])[0][0]
    return min_score_reg, spill_vars_for_reg[min_score_reg]

def get_reg_for_write(var, rd, ad, def_use, line_num, exclude):
    r = rd.get_reg_having_val_or_free(var)
    if r and (rd[r] == [var] or len(rd[r]) == 0):
        # r は var だけを持っているか、空っぽ
        return r, []

    spill_vars_for_reg = []
    for i in range(len(rd)):
        if i in exclude:
            continue
        reg = Register(i)
        spill_vars = list_remove_if_exist(get_spill_vars(reg, rd, ad, def_use, line_num), var)
        spill_vars_for_reg.append((reg, spill_vars))

    min_score_reg, min_score_spill_vars = sorted(spill_vars_for_reg, key=lambda elem: len(elem[1]))[0]
    return min_score_reg, min_score_spill_vars

def gen_asm_reg_for_read(var, rd, ad, def_use, line_num, exclude=[]):
    asm = []

    reg, spill_vars = get_reg_for_read(var, rd, ad, def_use, line_num, exclude)
    for spill_var in spill_vars:
        asm.append(ST(spill_var, reg))
        ad.spill(spill_var)
    if rd.assign_reg_for_read(reg, var):
        if isinstance(var, Number):
            asm.append(LI(reg, var))
        else:
            asm.append(LD(reg, var))
    ad.assign_reg_for_read(var, reg)

    return reg, asm

class BinOp(TAI):
    def __init__(self, dst, op, l, r, line_num):
        super().__init__(line_num)
        self._dst = dst
        self._op = op
        self._l = l
        self._r = r

    def __str__(self):
        return f'{self._dst} = {self._l} {self._op} {self._r}'

    @property
    def values(self):
        return {self._dst, self._l, self._r}

    @property
    def dst(self):
        return self._dst

    def gen_asm(self, rd, ad, def_use):
        asm = []

        l_reg, l_asm = gen_asm_reg_for_read(self._l, rd, ad, def_use, self._line_num)
        asm.extend(l_asm)
        r_reg, r_asm = gen_asm_reg_for_read(self._r, rd, ad, def_use, self._line_num, exclude=[l_reg.num])
        asm.extend(r_asm)

        d_reg, spill_vars = get_reg_for_write(self._dst, rd, ad, def_use, self._line_num, exclude=[l_reg.num, r_reg.num])
        for spill_var in spill_vars:
            asm.append(ST(spill_var, d_reg))

        rd.assign_reg_for_write(d_reg, self._dst)
        ad.assign_reg_for_write(self._dst, d_reg)
        asm.append(get_op(self._op)(d_reg, l_reg, r_reg))

        return asm

class Copy(TAI):
    def __init__(self, dst, src, line_num):
        super().__init__(line_num)
        self._dst = dst
        self._src = src

    def __str__(self):
        return f'{self._dst} = {self._src}'

    @property
    def values(self):
        return {self._dst, self._src}

    @property
    def dst(self):
        return self._dst

    def gen_asm(self, rd, ad, def_use):
        reg, asm = gen_asm_reg_for_read(self._src, rd, ad, def_use, self._line_num)
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

line_num = 0
def gen_line_num():
    global line_num
    l = line_num
    line_num += 1
    return l

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
        return BinOp(dst, binop, lhs, rhs, gen_line_num())
    elif len(elems) == 0:
        return Copy(dst, lhs, gen_line_num())

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
            return r_tac + [Copy(dst, r_var, gen_line_num())], dst
        else:
            l_tac, l_var = gen_tac_expr(lhs)
            r_tac, r_var = gen_tac_expr(rhs)
            dst = gen_tmp()
            return l_tac + r_tac + [BinOp(dst, expr.value, l_var, r_var, gen_line_num())], dst
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

def detect_def_use(tac, vars_alive_at_exit):
    # 変数が定義された行と、その値が最後に使用される行の組を、変数ごとに求める
    def_use = defaultdict(list)  # { 変数 : [ ( 定義行, 最終使用行 ) ] }
    last_def = dict() # 最終定義行
    last_use = dict() # 最終使用行

    for tai in tac:
        if isinstance(tai, BinOp) or isinstance(tai, Copy):
            def_var = tai.dst
        else:
            raise ValueError(f'unknown TAI type: {type(tai)}')

        use_vars = tai.values - {def_var}  # 式の右辺に現れる変数
        for var in use_vars:
            last_use[var] = tai.line_num

        # last_def を更新する前に last_def と last_use を使って def_use を計算
        if def_var in last_use:
            last_def_line = last_def.get(def_var, -1)
            def_use[def_var].append((last_def_line, last_use[def_var]))

        # def_use を計算し終えたら last_def を更新
        last_def[def_var] = tai.line_num

    for var in last_def:
        if var in vars_alive_at_exit:
            def_use[var].append((last_def[var], len(tac)))
        elif last_def[var] < last_use.get(var, 0):
            def_use[var].append((last_def[var], last_use[var]))

    # ブロック外で定義され、ブロック内で一度も変更されず、ブロックの出口で生存している変数に対する処理
    for var in vars_alive_at_exit:
        if var not in def_use:
            def_use[var].append((-1, len(tac)))

    return def_use

def main():
    give_tac_directly = True
    if give_tac_directly:
        src = '''\
_1 = a - b
_2 = a - c
_3 = 2 * _2
t = _1 + _3
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

    print('3 address code:')
    print('\n'.join(str(tai) for tai in three_addr_code))
    print()

    vars_alive_at_exit = {Variable(name) for name in ['a', 'b', 'c', 'd']}
    def_use = detect_def_use(three_addr_code, vars_alive_at_exit)

    variables = sorted((v for v in values if isinstance(v, Variable)), key=lambda v: v.name)
    temporaries = sorted((v for v in values if isinstance(v, Temporary)), key=lambda v: v.name)
    reg_num = 4

    gen = OneBlockCodeGenerator(reg_num, vars_alive_at_exit, def_use)

    verbose = False
    if verbose:
        info_printer = InfoTablePrinter(reg_num, variables, temporaries)
        info_printer.print_header()
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
    else:
        for tai in three_addr_code:
            asm = gen.gen_asm(tai)
            if asm:
                print('\n'.join(str(insn) for insn in asm))
        for insn in gen.gen_st_vars():
            print(insn)

if __name__ == '__main__':
    main()
