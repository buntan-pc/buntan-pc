#!/usr/bin/python3

'''
hex ファイルから defparam 行を生成する。

hex ファイルは 16 進数値が空白（改行）区切りで並んでいるのを想定。

pmem 用 hex ファイルの場合の例

    1C800
    3DEAD
    ...

生成される defparam 行は次のようになる。

    defparam xxx.INIT_RAM_00 = 288'h...72003DEAD;
    ...

`xxx` は BSRAM のインスタンス名で、<INSTANCE>_<num> というフォーマットになる。
- INSTANCE はコマンドライン引数で指定。
- num は連番（0～F）となる。
'''

from argparse import ArgumentParser
from itertools import islice
from math import ceil

def words(path, *args, **kwargs):
    with open(path, *args, **kwargs) as f:
        for line in f:
            for word in line.split():
                yield word

def hex2bin(h, digits):
    return ('{:0' + str(digits) + 'b}').format(int(h, base=16))

def main():
    p = ArgumentParser()
    p.add_argument('HEX_FILE', help='path to a hex file')
    p.add_argument('INSTANCE', help='base name of BSRAM instances')
    p.add_argument('--bit-width', '-b', default=18,
                   help='the width of each value in a hex file')
    p.add_argument('--gen-block', '-g', default=None,
                   help='name of the generator block including BSRAM instances')

    args = p.parse_args()

    line_bit_width = 256
    if (args.bit_width % 9) == 0:
        line_bit_width = 288

    w = words(args.HEX_FILE)

    loop = True
    line_num = 0
    while loop:
        w16 = islice(w, line_bit_width // args.bit_width)
        bin16 = [hex2bin(w, args.bit_width) for w in w16]
        line_bin = ''.join(reversed(bin16))
        if len(line_bin) < line_bit_width:
            line_bin = '0' * (line_bit_width - len(line_bin)) + line_bin
            loop = False
        line_hex = ''.join(f'{int(line_bin[i:i+4], base=2):X}' for i in range(0, line_bit_width, 4))
        # INIT_RAM_00 ～ INIT_RAM_3F
        inst_num = line_num // 0x40
        init_ram_i = line_num % 0x40

        inst = args.INSTANCE
        if args.gen_block:
            inst = f'{args.gen_block}[{inst_num}].{inst}'

        print(f"parameter {args.INSTANCE}_init_values [??] = {line_bit_width}'h" + line_hex + ';')
        line_num += 1

if __name__ == '__main__':
    main()
