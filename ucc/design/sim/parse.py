from io import BytesIO
import tokenize

NAME = 1
NUMBER = 2
OP = 3

class Node:
    def __init__(self, type, value, *children):
        self._type = type
        self._value = value
        self._children = children
        self._ershov = 0

    @property
    def type(self):
        return self._type

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

def program(ts):
    block = []
    while ts[0].type != tokenize.ENDMARKER:
        node, ts = expression(ts)
        if ts[0].type == tokenize.NEWLINE:
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
            node = Node(OP, t.string, node, rhs)
        else:
            break

    return node, ts

def additive(ts):
    node, ts = multiplicative(ts)

    while True:
        t = ts[0]
        if t.string in ['+', '-']:
            rhs, ts = multiplicative(ts[1:])
            node = Node(OP, t.string, node, rhs)
        else:
            break

    return node, ts

def multiplicative(ts):
    node, ts = primitive(ts)

    while True:
        t = ts[0]
        if t.string in ['*', '/']:
            rhs, ts = primitive(ts[1:])
            node = Node(OP, t.string, node, rhs)
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
    elif t.type == tokenize.NUMBER:
        node = Node(NUMBER, int(t.string))
        ts = ts[1:]
    elif t.type == tokenize.NAME:
        node = Node(NAME, t.string)
        ts = ts[1:]
    else:
        raise ValueError(f"unexpected token: '{ts[0].string}'. line='{ts[0].line}'")

    return node, ts

def parse(src):
    tokens = tokenize.tokenize(BytesIO(src.encode('utf-8')).readline)
    tokens = list(tokens)
    # tokens[0] は常に ENCODING なので読み飛ばす
    node, ts = program(tokens[1:])
    if ts[0].type not in [tokenize.ENDMARKER]:
        raise ValueError(f"unexpected token: '{ts[0].string}'. line='{ts[0].line}'")
    return node
