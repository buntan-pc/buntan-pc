#!/usr/bin/python3

import argparse
from bisect import bisect_right
from collections import namedtuple
from contextlib import nullcontext
import sys
import unittest


ADDRESS_COLUMN = "Parallel: Items"
DEBUG_COLUMN = "Function"
AddrLabel = namedtuple("AddrLabel", ["addr", "label"])
Record = namedtuple("Record", ["csv_line", "func"])


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


def add_debug_column(csv_file, addrlabels: list[AddrLabel], func_entry_only) -> list[Record]:
    addr_label_map = {}
    for addr, label in addrlabels:
        addr_label_map[addr] = label

    for line in csv_file:
        line = line.rstrip()
        # line: index,time,pmem_addr
        comma = line.rfind(",")
        # 観測される addr は実行中命令の次を指すので、
        # 実行中命令のアドレスを求めるために 1 を引く
        addr = int(line[comma+1:], 16) - 1
        if func_entry_only:
            func = addr_label_map.get(addr, None)
            if func is None:
                continue
        else:
            func = find_current_function(addr, addrlabels)
        yield Record(line, func)


def unique(records: list[Record]) -> list[Record]:
    prev_rec = next(records)
    rep_cnt = 1
    for r in records:
        if prev_rec.func == r.func:
            rep_cnt += 1
            continue
        else:
            if rep_cnt >= 2:
                yield Record(prev_rec.csv_line, prev_rec.func + f" [x{rep_cnt}]")
            else:
                yield prev_rec
            prev_rec = r
            rep_cnt = 1

    if rep_cnt >= 2:
        yield Record(prev_rec.csv_line, prev_rec.func + f" [x{rep_cnt}]")
    else:
        yield prev_rec


class TestUnique(unittest.TestCase):

    def test_ends_with_repeats(self):
        records = (Record(f"{i},2", func) for i, func in enumerate("abbbccc"))
        want = [
            Record("0,2", "a"),
            Record("1,2", "b [x3]"),
            Record("4,2", "c [x3]"),
        ]
        self.assertEqual(want, list(unique(records)))

    def test_ends_with_single(self):
        records = (Record(f"{i},2", func) for i, func in enumerate("abbbcccd"))
        want = [
            Record("0,2", "a"),
            Record("1,2", "b [x3]"),
            Record("4,2", "c [x3]"),
            Record("7,2", "d"),
        ]
        self.assertEqual(want, list(unique(records)))



def collapse_repeats(records: list[Record]) -> list[Record]:
    pattern = []  # [Record]
    repeat_candidate = []  # [Record]
    repeat_count = -1

    for r in records:
        if len(pattern) == 0:
            pattern.append(r)
            continue

        if repeat_count == -1:
            func_i = next(
                (i for i, elem in enumerate(pattern) if elem.func == r.func),
                -1
            )
            if func_i >= 0:
                # 繰り返しが始まった可能性
                for rec in pattern[:func_i]:
                    yield rec
                pattern = pattern[func_i:]
                if len(pattern) == 1 and pattern[0].func == r.func:
                    repeat_candidate = []
                    repeat_count = 1
                else:
                    repeat_candidate = [r]
                    repeat_count = 0
            else:
                if len(pattern) == 20:
                    yield pattern[0]
                    pattern.pop(0)
                pattern.append(r)
        elif repeat_count >= 0:
            if pattern[len(repeat_candidate)].func == r.func:
                # まだ繰り返しの可能性が続いている
                repeat_candidate.append(r)
                if len(pattern) == len(repeat_candidate):
                    repeat_candidate = []
                    repeat_count += 1
            elif repeat_count == 0:
                # 結局、繰り返しは無かった
                for rec in pattern:
                    yield rec
                pattern = repeat_candidate
                pattern.append(r)
                repeat_candidate = []
                repeat_count = -1
            else: # repeat_count >= 1
                # 繰り返しが終わった
                if len(pattern) == 1:
                    yield Record(pattern[0].csv_line, pattern[0].func + f" [x{repeat_count+1}]")
                else:
                    yield Record(f"[repeat x{repeat_count+1} total]", None)
                    for rec in pattern:
                        yield rec
                    yield Record("[/repeat]", None)

                for rec in repeat_candidate:
                    yield rec
                repeat_candidate = []
                pattern = [r]
                repeat_count = -1

    if repeat_count >= 0:
        yield Record(f"[repeat x{repeat_count+1} total]", None)
    for rec in pattern:
        yield rec
    if repeat_count >= 0:
        yield Record(f"[/repeat]", None)
    for rec in repeat_candidate:
        yield rec


class TestCollapseRepeats(unittest.TestCase):

    def test_true_candidate(self):
        records = [Record(f"{i},2", func) for i, func in enumerate("abababcdefefef")]
        want = [
            Record("[repeat x3 total]", None),
            Record("0,2", "a"),
            Record("1,2", "b"),
            Record("[/repeat]", None),
            Record("6,2", "c"),
            Record("7,2", "d"),
            Record("[repeat x3 total]", None),
            Record("8,2", "e"),
            Record("9,2", "f"),
            Record("[/repeat]", None),
        ]
        self.assertEqual(want, list(collapse_repeats(records)))

    def test_false_repeat_candidate(self):
        records = [Record(f"{i},2", func) for i, func in enumerate("abcdcecef")]
        want = [
            Record("0,2", "a"),
            Record("1,2", "b"),
            Record("2,2", "c"),
            Record("3,2", "d"),
            Record("[repeat x2 total]", None),
            Record("4,2", "c"),
            Record("5,2", "e"),
            Record("[/repeat]", None),
            Record("8,2", "f"),
        ]
        self.assertEqual(want, list(collapse_repeats(records)))

    def test_endswith_repeat(self):
        records = [Record(f"{i},2", func) for i, func in enumerate("abab")]
        want = [
            Record("[repeat x2 total]", None),
            Record("0,2", "a"),
            Record("1,2", "b"),
            Record("[/repeat]", None),
        ]
        self.assertEqual(want, list(collapse_repeats(records)))

    def test_endswith_candidate(self):
        records = [Record(f"{i},2", func) for i, func in enumerate("ababa")]
        want = [
            Record("[repeat x2 total]", None),
            Record("0,2", "a"),
            Record("1,2", "b"),
            Record("[/repeat]", None),
            Record("4,2", "a"),
        ]
        self.assertEqual(want, list(collapse_repeats(records)))

    def test_repeat_printing(self):
        records = [Record(f"{i},2", func) for i, func in enumerate("aaab")]
        want = [
            Record("0,2", "a [x3]"),
            Record("3,2", "b"),
        ]
        self.assertEqual(want, list(collapse_repeats(records)))


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
    parser.add_argument("--func-entry-only", action="store_true",
                        help="関数先頭の行だけを残す")
    parser.add_argument("--collapse-repeats", action="store_true",
                        help="同じ関数の組が連続する区間を省略する")

    args = parser.parse_args()

    with open(args.map) as f:
        addrlabels = load_addr_label_from_map(f)

    def open_or_use(file_path, open_mode, file_obj):
        if file_path and file_path != "-":
            return open(file_path, open_mode)
        return nullcontext(file_obj)

    with open_or_use(args.csv, "r", sys.stdin) as csv_file, \
         open_or_use(args.output, "w", sys.stdout) as out_file:
        header = csv_file.readline().strip()
        fieldnames = header.split(",")

        if ADDRESS_COLUMN not in fieldnames:
            raise RuntimeError(f"CSVに '{ADDRESS_COLUMN}' 列が見つかりません。")

        out_file.write(header + "," + DEBUG_COLUMN + "\n")

        records = add_debug_column(csv_file, addrlabels, args.func_entry_only)
        if args.collapse_repeats:
            records = unique(records)
            records = collapse_repeats(records)

        for r in records:
            out_file.write(r.csv_line + ("," + r.func if r.func else "") + "\n")


if __name__ == "__main__":
    main()
