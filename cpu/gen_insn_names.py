#!/usr/bin/env python3

import sys
from pathlib import Path


def load_entries(path: Path):
    entries = []
    for lineno, raw in enumerate(path.read_text().splitlines(), start=1):
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        parts = line.split()
        if len(parts) != 3:
            raise SystemExit(f"{path}:{lineno}: expected 'mask value name'")
        mask, value, name = parts
        entries.append((mask, value, name))
    return entries


def emit_cpp(entries):
    for mask, value, name in entries:
        print(f'  if (match_insn(insn, 0x{mask}, 0x{value})) return "{name}";')


def emit_sv(entries):
    if_ = "if"
    for mask, value, name in entries:
        print(f"    {if_} ((mcu.cpu.insn & 18'h{mask}) == 18'h{value})")
        print(f'      insn_name <= "{name}";')
        if_ = "else if"


def main():
    if len(sys.argv) != 3 or sys.argv[1] not in {"cpp", "sv"}:
        raise SystemExit("usage: gen_insn_names.py <cpp|sv> <insn_names.def>")

    mode = sys.argv[1]
    entries = load_entries(Path(sys.argv[2]))
    if mode == "cpp":
        emit_cpp(entries)
    else:
        emit_sv(entries)


if __name__ == "__main__":
    main()
