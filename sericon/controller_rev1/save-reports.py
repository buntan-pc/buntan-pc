#!/usr/bin/python3
"""impl/gwsynthesis と impl/pnr のレポート一式を
Git のコミット ID に紐付けて impl/reports/<name> 以下に保存する。

未コミットの変更がある場合:
  --label が無ければエラー
  --label が付いていればエラーにせず name="NO-COMMIT_<label>" として保存

未コミットの変更が無い場合:
  --label が無ければ name="<commit-id>" として保存
  --label があれば name="<commit-id>_<label>" として保存
  <commid-id> はコミット ID の先頭 7 文字
"""
import argparse
import shutil
import subprocess
import sys
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent
REPORT_DIRS = ['gwsynthesis', 'pnr']

# git status/diff は -C にどのディレクトリを渡してもリポジトリ直下からの相対パスで
# 結果を返してくるので、パス計算を全部リポジトリ直下基準に統一する。
REPO_ROOT = Path(subprocess.run(
    ['git', '-C', str(ROOT), 'rev-parse', '--show-toplevel'],
    capture_output=True, text=True, check=True,
).stdout.strip())
PATHSPEC = ROOT.relative_to(REPO_ROOT).as_posix()


def perror(*args):
    print(*args, file=sys.stderr)


def run_git_oneline(*args):
    """1 行の値（コミットハッシュ、ブランチ名など）だけを返すコマンド用。
    stdout を strip するので、複数行や空白が意味を持つ出力には使わないこと。
    """
    return subprocess.run(
        ['git', '-C', str(REPO_ROOT), *args],
        capture_output=True, text=True, check=True,
    ).stdout.strip()


def git_status():
    """追跡されているファイルの状態だけを返す（未追跡ファイルは無視する）。"""
    out = subprocess.run(
        ['git', '-C', str(REPO_ROOT), 'status', '--porcelain', '--', PATHSPEC],
        capture_output=True, text=True, check=True,
    ).stdout
    return ''.join(line + '\n' for line in out.splitlines() if not line.startswith('??'))


def ensure_build_is_fresh():
    """impl/ 以下が現在のソースと同期しているか、make の依存関係グラフに
    ただ乗りして確認する（実際にビルドはしない: make -q は mtime を見るだけ）。
    """
    result = subprocess.run(
        ['make', '-q'],
        cwd=str(ROOT), capture_output=True, text=True,
    )
    if result.returncode != 0:
        perror('impl/ 以下が現在のソースと同期していない可能性があります。先に `make` を実行してください。')
        if result.stdout:
            perror(result.stdout)
        if result.stderr:
            perror(result.stderr)
        sys.exit(1)


def build_uncommitted_diff():
    """追跡されているファイルの差分（HEAD との比較）をパッチとして返す。"""
    return subprocess.run(
        ['git', '-C', str(REPO_ROOT), 'diff', 'HEAD', '--', PATHSPEC],
        capture_output=True, text=True, check=True,
    ).stdout


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--label', help='保存先ディレクトリ名に付ける識別子（例: clock_uncertainty）')
    ap.add_argument('--force', action='store_true', help='保存先が既に存在していても上書きする')
    args = ap.parse_args()

    ensure_build_is_fresh()

    status = git_status()
    dirty = bool(status)

    if dirty and not args.label:
        perror('未コミットの変更があります。コミットするか --label を付けて実行してください。')
        perror(status)
        sys.exit(1)

    commit = run_git_oneline('rev-parse', 'HEAD')
    branch = run_git_oneline('rev-parse', '--abbrev-ref', 'HEAD')
    subject = run_git_oneline('log', '-1', '--format=%s')

    if dirty:
        dest_name = f'NO-COMMIT_{args.label}'
        perror(f'未コミットの変更があるため "{dest_name}" として保存します')
    else:
        short = commit[:7]
        dest_name = f'{short}_{args.label}' if args.label else short

    dest = ROOT / 'impl' / 'reports' / dest_name

    if dest.exists():
        if not args.force:
            perror(f'{dest} は既に存在します（--force で上書き）')
            sys.exit(1)
        shutil.rmtree(dest)

    dest.mkdir(parents=True)

    for name in REPORT_DIRS:
        src = ROOT / 'impl' / name
        if not src.is_dir():
            perror(f'{src} が見つかりません。先にビルドしてください。')
            sys.exit(1)
        shutil.copytree(src, dest / name)

    info = (
        f'commit: {commit} ({"dirty" if dirty else "clean"})\n'
        f'branch: {branch}\n'
        f'subject: {subject}\n'
        f'saved_at: {datetime.now().isoformat(timespec="seconds")}\n'
    )
    if dirty:
        info += f'\n--- git status --porcelain ---\n{status}'
        info += '\n差分は UNCOMMITTED.diff を参照（git apply で当てられる形式）\n'
        (dest / 'UNCOMMITTED.diff').write_text(build_uncommitted_diff())
    (dest / 'COMMIT_INFO.txt').write_text(info)

    print(f'saved to {dest}')


if __name__ == '__main__':
    main()
