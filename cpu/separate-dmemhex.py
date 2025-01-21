#!/usr/bin/python3

import re
import sys

LINE_PAT = re.compile('^([0-9A-F]{2})([0-9A-F]{2})$')

def main():
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <output basename>')
        sys.exit(1)
    basename = sys.argv[1]

    hi = open(f'{basename}_hi.hex', 'w')
    lo = open(f'{basename}_lo.hex', 'w')

    for line in sys.stdin:
        m = LINE_PAT.match(line)
        if not m:
            print(f'failed to parse a line: {line}', file=sys.stderr)
            sys.exit(1)
        hi.write(m.group(1) + '\n')
        lo.write(m.group(2) + '\n')

if __name__ == '__main__':
    main()
