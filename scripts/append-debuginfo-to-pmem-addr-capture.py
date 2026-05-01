#!/usr/bin/python3

import argparse
from bisect import bisect_right
from collections import namedtuple
from contextlib import nullcontext
import sys


ADDRESS_COLUMN = "Parallel: Items"
DEBUG_COLUMN = "Function"
AddrLabel = namedtuple("AddrLabel", ["addr", "label"])


def load_addr_label_from_map(map_file):
    """
    mapファイルから section .text のラベルだけを読み込む。

    対応するmap形式:

        section .data
        ADDR LABEL
        ----------
        0000 pmem_len
        0002 dmem_len

        section .text
        ADDR LABEL
        ----------
        0000 start
        0002 fin
        0003 buntan_main

    戻り値:
        [
          AddrLabel(0, "start"),
          AddrLabel(2, "fin"),
          AddrLabel(3, "buntan_main")
        ]
    """
    entries = []

    # section .text まで読み飛ばす
    for line in map_file:
        if line.startswith("section .text"):
            break
    # ---- まで読み飛ばす
    for line in map_file:
        if line.startswith("----"):
            break

    for line in map_file:
        line = line.strip()
        parts = line.split()

        if len(parts) != 2:
            raise ValueError("invalid syntax of map line:", line)

        entries.append(AddrLabel(int(parts[0], 16), parts[1]))

    return entries


def find_current_function(addr: int, addrlabels) -> str:
    """
    addr 以下で最も近いラベルを返す。

    例:
        map:
            0000 start
            0002 fin
            0003 buntan_main

        addr=0000 -> start
        addr=0001 -> start
        addr=0002 -> fin
        addr=0003 -> buntan_main
        addr=0004 -> buntan_main
    """
    index = bisect_right(addrlabels, addr, key=lambda x: x.addr) - 1
    if index < 0:
        return ""
    return addrlabels[index].label


def add_debug_column(csv_file, out_file, addrlabels):
    header = csv_file.readline().strip()
    fieldnames = header.split(",")

    if ADDRESS_COLUMN not in fieldnames:
        raise RuntimeError(f"CSVに '{ADDRESS_COLUMN}' 列が見つかりません。")

    out_file.write(header + "," + DEBUG_COLUMN + "\n")

    for line in csv_file:
        line = line.rstrip()
        # line: index,time,=pmem_addr
        comma = line.rfind(",")
        addr = int(line[comma+1:], 16)
        func = find_current_function(addr, addrlabels)
        out_file.write(line + "," + func + "\n")


def main():
    parser = argparse.ArgumentParser(
        description="""\
ロジックアナライザで記録した CSV 中の pmem アドレスに対応する関数名を追加する。
CSV ファイルのフォーマット：
    Id,Time[ns],Parallel: Items
    1,20.00,F1E
    2,160.00,018
    3,300.00,019

"Parallel: Items" が pmem アドレスである。

pmem アドレスから関数名を特定するために map ファイルを利用する。
map ファイルのフォーマット：
    section .data
    ADDR LABEL
    ----------
    0000 pmem_len
    0002 dmem_len

    section .text
    ADDR LABEL
    ----------
    0000 start
    0002 fin
    0003 buntan_main
""",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )

    parser.add_argument("csv", help="入力 CSV ファイル")
    parser.add_argument("map", help="uas 出力の map ファイル")
    parser.add_argument("-o", "--output", help="出力 CSV ファイル")

    args = parser.parse_args()

    with open(args.map) as f:
        addrlabels = load_addr_label_from_map(f)

    def open_or_use(file_path, open_mode, file_obj):
        if file_path and file_path != "-":
            return open(file_path, open_mode)
        return nullcontext(file_obj)

    with open_or_use(args.csv, "r", sys.stdin) as csv_file, \
         open_or_use(args.output, "w", sys.stdout) as out_file:
        add_debug_column(csv_file, out_file, addrlabels)


if __name__ == "__main__":
    main()
