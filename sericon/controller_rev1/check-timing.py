#!/usr/bin/python3
"""impl/pnr のタイミングレポートを要約して表示する。

デフォルトでは違反があっても表示するだけで exit 0 する。
--strict を付けると違反があるとき exit 1 する（将来ビルドを止めたくなったら
Makefile の呼び出しに --strict を足すだけでよい）。
"""
import argparse
import html
import re
import sys
from pathlib import Path

RED = '\033[31m'
GREEN = '\033[32m'
BOLD = '\033[1m'
RESET = '\033[0m'


def read_violated_counts(tr_content: Path) -> dict[str, int | None]:
    """controller_tr_content.html から違反エンドポイント数を読む（全パス対象の集計）"""
    text = tr_content.read_text(encoding='utf-8', errors='replace')
    plain = html.unescape(re.sub(r'<[^>]+>', '|', text))
    counts = {}
    for kind in ('Setup', 'Hold'):
        m = re.search(kind + r' Violated Endpoints[\s|]+(\d+)', plain)
        counts[kind] = int(m.group(1)) if m else None
    return counts


def read_worst_paths(timing_paths: Path) -> dict[str, list[tuple[float, str, str]]]:
    """controller.timing_paths からパス種別ごとの (slack, from, to) を読む（ワースト25本のみ）"""
    paths: dict[str, list[tuple[float, str, str]]] = {}
    for block in timing_paths.read_text().split('====='):
        lines = [l.strip() for l in block.strip().splitlines() if l.strip()]
        if len(lines) < 3:
            continue
        ptype = lines[0]
        try:
            slack = float(lines[1])
        except ValueError:
            continue
        nodes = []
        for line in lines[2:]:
            try:
                float(line)
            except ValueError:
                nodes.append(line)
        if not nodes:
            continue
        paths.setdefault(ptype, []).append((slack, nodes[0], nodes[-1]))
    return paths


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--strict', action='store_true', help='違反があるとき exit 1 する')
    ap.add_argument('--pnr-dir', default=Path(__file__).resolve().parent / 'impl' / 'pnr', type=Path)
    ap.add_argument('--max-list', default=5, type=int, help='表示する違反パスの最大本数')
    args = ap.parse_args()

    tr_content = args.pnr_dir / 'controller_tr_content.html'
    timing_paths = args.pnr_dir / 'controller.timing_paths'
    if not tr_content.exists() or not timing_paths.exists():
        print(f'check-timing: レポートが見つかりません ({args.pnr_dir})', file=sys.stderr)
        return 0 if not args.strict else 1

    counts = read_violated_counts(tr_content)
    paths = read_worst_paths(timing_paths)
    use_color = sys.stdout.isatty()

    def paint(s: str, color: str) -> str:
        return f'{color}{s}{RESET}' if use_color else s

    print(f'=== タイミング要約 ({args.pnr_dir}) ===')
    violated = False
    for kind, ptype in (('Setup', 'SETUP'), ('Hold', 'HOLD')):
        plist = paths.get(ptype, [])
        worst = min((p[0] for p in plist), default=None)
        n_violated = counts.get(kind)
        ok = (n_violated == 0) if n_violated is not None else (worst is None or worst >= 0)
        mark = paint('OK', GREEN) if ok else paint('** VIOLATED **', RED + BOLD)
        worst_s = f'{worst:+.3f} ns' if worst is not None else 'N/A'
        count_s = f'{n_violated}' if n_violated is not None else '?'
        print(f'{kind:5}: worst slack {worst_s}, 違反エンドポイント数 {count_s}  {mark}')
        if not ok:
            violated = True
            for slack, frm, to in [p for p in plist if p[0] < 0][: args.max_list]:
                print(f'    {slack:+.3f}  {frm} -> {to}')
            rest = len([p for p in plist if p[0] < 0]) - args.max_list
            if rest > 0:
                print(f'    ... 他 {rest} 本 (レポート上位25本中)')

    if violated:
        print(paint('警告: タイミング違反があります。ビットストリームは生成されていますが実機で動かない可能性があります。', RED))
        print(f'詳細: {tr_content}')

    return 1 if (violated and args.strict) else 0


if __name__ == '__main__':
    sys.exit(main())
