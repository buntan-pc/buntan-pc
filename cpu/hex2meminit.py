#!/usr/bin/python3

'''
hex ファイルから初期値の配列を生成する。

hex ファイルは 16 進数値が空白（改行）区切りで並んでいるのを想定。
pmem 用 hex ファイルの場合の例:

    1C800
    3DEAD
    ...

生成される配列は次のようになる。

    parameter [288-1:0] INSTANCE_init_values [...:0] = '{
      default: 288'h0,
      0: 288'h...72003DEAD,
      1: 288'h...
    };
    ...
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

def make_init_values(hex_words, line_bit_width, word_bit_width):
    init_values = []

    while True:
        hw16 = islice(hex_words, line_bit_width // word_bit_width)
        bw16 = [hex2bin(hw, word_bit_width) for hw in hw16]
        line_bin = ''.join(reversed(bw16))
        if len(line_bin) == 0:
            break
        if len(line_bin) < line_bit_width:
            line_bin = '0' * (line_bit_width - len(line_bin)) + line_bin

        line_hex = ''.join(f'{int(line_bin[i:i+4], base=2):X}'
                           for i in range(0, line_bit_width, 4))
        init_values.append(f"{line_bit_width}'h" + line_hex)

    return init_values

def main():
    p = ArgumentParser()
    p.add_argument('HEX_FILE', help='path to a hex file')
    p.add_argument('INSTANCE', help='base name of BSRAM instances')
    p.add_argument('LENGTH',   type=int,
                   help='the number of init values')
    p.add_argument('--bit-width', '-b', default=18,
                   help='the width of each value in a hex file')
    p.add_argument('--gen-block', '-g', default=None,
                   help='name of the generator block including BSRAM instances')

    args = p.parse_args()

    line_bit_width = 256
    if (args.bit_width % 9) == 0:
        line_bit_width = 288

    init_values = make_init_values(words(args.HEX_FILE),
                                   line_bit_width,
                                   args.bit_width)
    array_name = f'{args.INSTANCE}_init_values'
    print(f'''\
parameter bit [{line_bit_width}-1:0] {array_name} [{args.LENGTH}-1:0] = '{{
  default: {line_bit_width}'h0,
  {',\n  '.join(f'{i}:{v}' for i,v in enumerate(init_values))}
}};''')

if __name__ == '__main__':
    main()
