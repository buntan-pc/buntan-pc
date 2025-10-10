from collections import namedtuple

def make_insn_class(name, params):
    def __str__(self):
        operands = ', '.join(str(getattr(self, f)) for f in self._fields)
        return f'{name} {operands}'
    base_insn = namedtuple(name, params)
    cls = type(name, (base_insn,), {"__str__": __str__})
    return cls

LD = make_insn_class('LD', ['rd', 'name'])
ST = make_insn_class('ST', ['name', 'rd'])
LI = make_insn_class('LI', ['rd', 'imm'])
ADD = make_insn_class('ADD', ['rd', 'rs1', 'rs2'])
SUB = make_insn_class('SUB', ['rd', 'rs1', 'rs2'])
MUL = make_insn_class('MUL', ['rd', 'rs1', 'rs2'])
DIV = make_insn_class('DIV', ['rd', 'rs1', 'rs2'])

def get_op(op):
    return {
        '+': ADD,
        '-': SUB,
        '*': MUL,
        '/': DIV
    }[op]
