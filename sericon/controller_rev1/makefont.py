#!/usr/bin/python3

import argparse
import collections
import functools
import re
import sys


BITMAP_PATTERN = re.compile(r'([.*@]+)')


def compile_to_hex(src: str) -> str:
    src = src.lstrip()
    result = []

    for line in src.splitlines():
        m = BITMAP_PATTERN.match(line)
        if not m:
            continue

        bits = [(0 if x == '.' else 1) for x in m.group(1)]
        bits_int = functools.reduce(lambda a, b: 2*a + b, bits)
        result.append('{:02X}\n'.format(bits_int))

    return ''.join(result)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('font', help='path to a font file')
    parser.add_argument('-o', help='path to an output file', default='font.hex')
    ns = parser.parse_args()

    with open(ns.o, 'w') as out, open(ns.font) as font:
        src = font.read()
        out.write(compile_to_hex(src))


if __name__ == '__main__':
    main()
