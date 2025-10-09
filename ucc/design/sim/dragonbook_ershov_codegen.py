#!/usr/bin/python3

from io import BytesIO
from tokenize import tokenize, NAME, NUMBER, NEWLINE, ENDMARKER

class Node:
    def __init__(self, value, *children):
        self._value = value
        self._children = children
        self._ershov = 0

    @property
    def value(self):
        return self._value

    @property
    def children(self):
        return self._children

    @property
    def ershov(self):
        return self._ershov

    @ershov.setter
    def ershov(self, n):
        self._ershov = n

def calc_ershov_number(expr):
    if (expr.children is None) or len(expr.children) == 0:
        expr.ershov = 1
        return 1
    elif len(expr.children) == 2:
        lhs, rhs = expr.children
        l = calc_ershov_number(lhs)
        r = calc_ershov_number(rhs)
        if l == r:
            expr.ershov = l + 1
            return l + 1
        elif l < r:
            expr.ershov = r
            return r
        else:
            expr.ershov = l
            return l
    else:
        raise ValueError('expr must be a leaf or a binary expression')

def print_tree(t, name, indent=0):
    print(f'{" "*(indent*2)}{t.value} [name={name} ershov={t.ershov}]')
    if t.children is not None:
        if len(t.children) == 2:
            lhs, rhs = t.children
            print_tree(lhs, 'L', indent + 1)
            print_tree(rhs, 'R', indent + 1)
        else:
            for i, c in enumerate(t.children):
                print_tree(c, str(i), indent + 1)

def gen_asm_for_expr(expr, base=0):
    if len(expr.children) == 0:
        if isinstance(expr.value, int):
            return [f'MOV R{base + expr.ershov}, {expr.value}']
        else:
            return [f'LD R{base + expr.ershov}, {expr.value}']
    elif len(expr.children) == 2:
        lhs, rhs = expr.children
        r_result = base + rhs.ershov
        l_result = base + lhs.ershov
        asm = []
        if lhs.ershov == rhs.ershov:
            asm.extend(gen_asm_for_expr(rhs, base + 1))
            asm.extend(gen_asm_for_expr(lhs, base))
            r_result += 1
        elif lhs.ershov < rhs.ershov:
            asm.extend(gen_asm_for_expr(rhs, base))
            asm.extend(gen_asm_for_expr(lhs, base))
        else: # lhs.ershov > rhs.ershov
            asm.extend(gen_asm_for_expr(lhs, base))  # Ershov 数が大きい左辺を先に生成
            asm.extend(gen_asm_for_expr(rhs, base))

        return asm + [f'{expr.value} R{base + expr.ershov}, R{l_result}, R{r_result}']
    else:
        raise ValueError('expr must be a leaf or a binary expression')

def program(ts):
    block = []
    while ts[0].type != ENDMARKER:
        node, ts = expression(ts)
        if ts[0].type == NEWLINE:
            ts = ts[1:]
        else:
            raise ValueError(f"newline is expected, got '{ts[0].string}'. line='{ts[0].line}'")
        block.append(node)

    return block, ts

def expression(ts):
    return assignment(ts)

def assignment(ts):
    node, ts = additive(ts)

    while True:
        t = ts[0]
        if t.string == '=':
            rhs, ts = additive(ts[1:])
            node = Node(t.string, node, rhs)
        else:
            break

    return node, ts

def additive(ts):
    node, ts = multiplicative(ts)

    while True:
        t = ts[0]
        if t.string in ['+', '-']:
            rhs, ts = multiplicative(ts[1:])
            node = Node(t.string, node, rhs)
        else:
            break

    return node, ts

def multiplicative(ts):
    node, ts = primitive(ts)

    while True:
        t = ts[0]
        if t.string in ['*', '/']:
            rhs, ts = primitive(ts[1:])
            node = Node(t.string, node, rhs)
        else:
            break

    return node, ts

def primitive(ts):
    t = ts[0]
    if t.string == '(':
        node, ts = expression(ts[1:])
        if ts[0].string != ')':
            raise ValueError(f"')' is expected, got '{ts[0].string}'. line='{ts[0].line}'")
        ts = ts[1:]
    elif t.type == NUMBER:
        node = Node(int(t.string))
        ts = ts[1:]
    elif t.type == NAME:
        node = Node(t.string)
        ts = ts[1:]
    else:
        raise ValueError(f"unexpected token: '{ts[0].string}'. line='{ts[0].line}'")

    return node, ts

def parse(tokens):
    # tokens[0] は常に ENCODING なので読み飛ばす
    node, ts = program(tokens[1:])
    if ts[0].type not in [ENDMARKER]:
        raise ValueError(f"unexpected token: '{ts[0].string}'. line='{ts[0].line}'")
    return node

def main():
    src = '''\
t = (a - b) + (a - c) + (a - c)
a = d
d = t
'''

    print('src:')
    print(src)

    tokens = tokenize(BytesIO(src.encode('utf-8')).readline)
    prog = parse(list(tokens))

    for expr in prog:
        if expr.value == '=':  # assignment
            lhs, rhs = expr.children
            calc_ershov_number(rhs)
            print('\n'.join(gen_asm_for_expr(rhs)))
            print(f'ST {lhs.value}, R{rhs.ershov}')

if __name__ == '__main__':
    main()
