#!/usr/bin/python3
"""Summarize Gowin PnR resources, comparing with the committed HEAD entry.
PnR レポートから FPGA リソース使用量（Logic・Register・CLS）を取得し、表示する。
最後のコミット（HEAD）からの使用量の増減も表示する。

リソース使用量の履歴を resource-history.json に記録する。
FPGA の SystemVerilog コードと一緒にコミットすること。
未ビルドの変更が残ったままコミットすると、resource-history.json に記録された
リソース使用量と実際の値がずれるので、必ずコミット前に make を実行する。

- resource-history.json の "HEAD" は最新の make 時点のリソース使用量を記録
- make 実行時、**コミット済みファイル** の "HEAD" 値を取得し、
  実際のコミットハッシュをキーとして resource-hisotory.json に追加
- コミットせずに何度ビルドしても問題ない。

テスト方法（このファイルのあるディレクトリで実行）:
    python3 -m unittest discover -s tests -p 'test_resource_usage.py'
"""

import argparse
from html.parser import HTMLParser
import json
from pathlib import Path
import re
import subprocess
import sys


RESOURCES = ("Logic", "Register", "CLS")


class ReportParser(HTMLParser):
    def __init__(self):
        super().__init__()
        self.rows = []
        self.row = []
        self.cell = None

    def handle_starttag(self, tag, attrs):
        if tag == "tr":
            self.row = []
        elif tag in ("td", "th"):
            self.cell = []

    def handle_data(self, data):
        if self.cell is not None:
            self.cell.append(data)

    def handle_endtag(self, tag):
        if tag in ("td", "th") and self.cell is not None:
            self.row.append("".join(self.cell).strip())
            self.cell = None
        elif tag == "tr":
            self.rows.append(self.row)


def parse_report(text):
    parser = ReportParser()
    parser.feed(text)
    resources = {}
    for row in parser.rows:
        if len(row) < 2 or row[0] not in RESOURCES:
            continue
        match = re.fullmatch(r"(\d+)\s*/\s*(\d+)", row[1])
        if not match:
            raise ValueError(f"Invalid usage for {row[0]}: {row[1]}")
        used, total = map(int, match.groups())
        if total <= 0 or used > total:
            raise ValueError(f"Invalid usage for {row[0]}: {row[1]}")
        resources[row[0]] = {"used": used, "total": total}
    if set(resources) != set(RESOURCES):
        raise ValueError("PnR report is missing resource usage rows")
    return resources


def read_history(text):
    history = json.loads(text)
    if not isinstance(history, dict):
        raise ValueError("Resource history must be a JSON object")
    for revision, resources in history.items():
        if revision != "HEAD" and not re.fullmatch(r"[0-9a-f]{40}|[0-9a-f]{64}", revision):
            raise ValueError(f"Invalid history revision: {revision}")
        if not isinstance(resources, dict) or set(resources) != set(RESOURCES):
            raise ValueError(f"Invalid resources for {revision}")
        for name, counts in resources.items():
            if (not isinstance(counts, dict)
                    or set(counts) != {"used", "total"}
                    or any(type(n) is not int for n in counts.values())
                    or not 0 <= counts["used"] <= counts["total"]
                    or counts["total"] <= 0):
                raise ValueError(f"Invalid {name} counts for {revision}")
    return history


def git(directory, *args):
    return subprocess.run(
        ["git", "-C", str(directory), *args],
        text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    ).stdout.strip()


def update_history(report, history_path):
    current = parse_report(report.read_text(encoding="utf-8"))
    history_path = history_path.resolve()
    root = Path(git(history_path.parent, "rev-parse", "--show-toplevel"))
    commit = git(root, "rev-parse", "HEAD")
    relative_path = history_path.relative_to(root).as_posix()
    # Read the committed file, never promote the working copy's HEAD entry.
    tracked = git(root, "ls-tree", "--name-only", commit, "--", relative_path)
    committed = read_history(git(root, "show", f"{commit}:{relative_path}")) if tracked else {}
    history = read_history(history_path.read_text(encoding="utf-8")) if history_path.exists() else {}
    history.update({key: value for key, value in committed.items() if key != "HEAD"})
    baseline = committed.get("HEAD")
    if baseline is not None:
        history[commit] = baseline
    history["HEAD"] = current

    output = json.dumps(history, indent=2, sort_keys=True) + "\n"
    if not history_path.exists() or history_path.read_text(encoding="utf-8") != output:
        temporary = history_path.with_suffix(history_path.suffix + ".tmp")
        temporary.write_text(output, encoding="utf-8")
        temporary.replace(history_path)

    print(f"FPGA resource usage (delta vs HEAD {commit[:12]})")
    print(f"{'Resource':<10} {'Used':>5} / {'Total':>5} {'Usage':>8} {'Delta':>8}")
    for name in RESOURCES:
        used, total = current[name]["used"], current[name]["total"]
        delta = "N/A"
        if baseline is not None and baseline[name]["total"] == total:
            delta = f"{used - baseline[name]['used']:+d}"
        print(f"{name:<10} {used:>5} / {total:>5} {used / total:>8.1%} {delta:>8}")
    if baseline is None:
        print("No committed HEAD resource entry yet; commit the history with the sources.")
    elif any(baseline[name]["total"] != current[name]["total"] for name in RESOURCES):
        print("N/A: resource capacity changed since HEAD.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("report", type=Path)
    parser.add_argument("history", type=Path)
    args = parser.parse_args()
    try:
        update_history(args.report, args.history)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Resource summary failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
