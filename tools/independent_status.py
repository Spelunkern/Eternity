#!/usr/bin/env python3
"""Show independent worktree heads, changes and delivered reports."""
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT.parent / 'Eternity-independent'


def main():
    if not BASE.is_dir():
        print('No independent Eternity worktrees found.')
        return
    for tree in sorted(path for path in BASE.iterdir() if path.is_dir()):
        def git(*args):
            return subprocess.check_output(['git', '-C', str(tree), *args], text=True).strip()
        try:
            branch = git('branch', '--show-current')
            head = git('rev-parse', '--short', 'HEAD')
            changes = len(git('status', '--porcelain').splitlines())
            report = tree / 'reports' / 'independent' / (tree.name + '.md')
            print(f'{tree.name:20} {branch:30} {head}  changes={changes:3}  report={"present" if report.is_file() else "pending"}')
        except subprocess.CalledProcessError:
            print(f'{tree.name:20} unable to inspect worktree')


if __name__ == '__main__':
    main()
