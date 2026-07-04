#!/usr/bin/python3
'''
USB の CRC5 生成アルゴリズムの Python 版
See: https://www.usb.org/sites/default/files/crcdes.pdf
'''

import sys


def xor5(x, y):
    return ['0' if a == b else '1' for a, b in zip(x[:5], y[:5])]


def crc5(st_data: str) -> str:
    # USB CRC5 generator polynomial bits used by the original Perl code.
    # Perl: @G = ('0','0','1','0','1')
    g = ['0', '0', '1', '0', '1']

    data = list(st_data)
    hold = ['1', '1', '1', '1', '1']

    while data:
        nextb = data.pop(0)
        print(f'{nextb}  {int(hold[0] == nextb)}  {" ".join(hold)}')

        # Ignore non-binary characters, matching the Perl "next loop5".
        if nextb not in ('0', '1'):
            continue

        old_msb = hold.pop(0)

        if nextb == old_msb:
            hold.append('0')
        else:
            hold.append('0')
            hold = xor5(hold, g)

    # Invert shift register contents to generate CRC field.
    return ''.join('0' if b == '1' else '1' for b in hold)


def main() -> None:
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} nrzstream", file=sys.stderr)
        sys.exit(2)

    print(crc5(sys.argv[1]))


if __name__ == '__main__':
    main()
