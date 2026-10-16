#!/usr/bin/python3
"""カレントディレクトリの impl/pnr のタイミングレポートを要約して表示する。

*_tr_content.html を自動検出し、同名の .timing_paths と HTML の Recovery Paths Table を読む。
複数ある場合は更新日時が最新のものを採用する（同日時ならファイル名の昇順）。
--pnr-dir で検索先のディレクトリを変更できる。

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
    """*_tr_content.html の STA 概要にある Setup/Hold 違反数を読む。"""
    text = tr_content.read_text(encoding='utf-8', errors='replace')
    plain = html.unescape(re.sub(r'<[^>]+>', '|', text))
    counts = {}
    for kind in ('Setup', 'Hold'):
        m = re.search(kind + r' Violated Endpoints[\s|]+(\d+)', plain)
        counts[kind] = int(m.group(1)) if m else None
    return counts


def read_html_paths(tr_content: Path, kind: str) -> list[tuple[float, str, str]]:
    """*_tr_content.html の指定スラック表から (slack, from, to) を読む。"""
    anchor = f'{kind}_Slack_Table'
    text = tr_content.read_text(encoding='utf-8', errors='replace')
    section = re.search(
        rf'<h3><a name="{re.escape(anchor)}">.*?</table>', text, re.DOTALL
    )
    if not section:
        return []

    paths = []
    for row in re.finditer(r'<tr\b[^>]*>(.*?)</tr>', section.group(), re.DOTALL):
        cells = [html.unescape(re.sub(r'<[^>]+>', '', cell)).strip()
                 for cell in re.findall(r'<td\b[^>]*>(.*?)</td>', row.group(1), re.DOTALL)]
        if len(cells) < 4:
            continue
        try:
            slack = float(cells[1])
        except ValueError:
            continue
        paths.append((slack, cells[2], cells[3]))
    return paths


def read_worst_paths(timing_paths: Path) -> dict[str, list[tuple[float, str, str]]]:
    """*.timing_paths からパス種別ごとの (slack, from, to) を読む（ワースト25本のみ）"""
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
    ap.add_argument('--pnr-dir', default=Path('impl/pnr'), type=Path,
                    help='レポートの検索先（デフォルト: カレントディレクトリの impl/pnr）')
    ap.add_argument('--max-list', default=5, type=int, help='表示する違反パスの最大本数')
    args = ap.parse_args()

    reports = sorted(p for p in args.pnr_dir.glob('*_tr_content.html') if p.is_file())
    if not reports:
        print(f'check-timing: レポートが見つかりません ({args.pnr_dir})', file=sys.stderr)
        return 0 if not args.strict else 1
    tr_content = max(reports, key=lambda p: p.stat().st_mtime_ns)
    if len(reports) > 1:
        print(f'check-timing: 更新日時が最新のレポートを採用します ({tr_content})',
              file=sys.stderr)
    project_name = tr_content.name.removesuffix('_tr_content.html')
    timing_paths = args.pnr_dir / f'{project_name}.timing_paths'
    if not timing_paths.is_file():
        print(f'check-timing: レポートが見つかりません ({timing_paths})', file=sys.stderr)
        return 0 if not args.strict else 1

    counts = read_violated_counts(tr_content)
    paths = read_worst_paths(timing_paths)
    use_color = sys.stdout.isatty()

    def paint(s: str, color: str) -> str:
        return f'{color}{s}{RESET}' if use_color else s

    print(f'=== タイミング要約 ({args.pnr_dir}) ===')
    violated = False
    for kind, ptype in (('Setup', 'SETUP'), ('Hold', 'HOLD'), ('Recovery', 'RECOVERY')):
        plist = paths.get(ptype, [])
        if kind == 'Recovery':
            plist = read_html_paths(tr_content, 'Recovery')
        worst = min((p[0] for p in plist), default=None)
        n_violated = sum(p[0] < 0 for p in plist)
        ok = worst is None or worst >= 0
        mark = paint('OK', GREEN) if ok else paint('** VIOLATED **', RED + BOLD)
        worst_s = f'{worst:+.3f} ns' if worst is not None else 'N/A'
        count_s = f'{n_violated}/{len(plist)} (表示された経路)'
        print(f'{kind:8}: worst slack {worst_s}, 違反経路数 {count_s}  {mark}')
        summary_count = counts.get(kind)
        if kind == 'Setup' and summary_count not in (None, n_violated):
            print(f'          STA 概要の Setup 違反エンドポイント数は {summary_count} '
                  '（Setup パス表の結果と不一致）')
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
