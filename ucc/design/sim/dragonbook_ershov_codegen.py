#!/usr/bin/python3

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

def main():
    expr = Node('+', Node('*', Node(2), Node(3)), Node('*', Node(3), Node('+', Node(4), Node(5))))
    calc_ershov_number(expr)
    print_tree(expr, 'root')
    print('\n'.join(gen_asm_for_expr(expr)))

if __name__ == '__main__':
    main()
