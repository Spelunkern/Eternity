#!/usr/bin/env python3
"""Build and run one CTest target in an Eternity worktree under the shared lock."""
import argparse
from pathlib import Path
import re
import subprocess
import uuid
from parallel_lock import resource_lock


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('worktree', type=Path)
    parser.add_argument('target', help='CMake executable and identically named CTest test')
    args = parser.parse_args()
    root = args.worktree.resolve(strict=True)
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_-]*', args.target):
        parser.error('target must be a simple CMake target name')
    actual = subprocess.check_output(['git', '-C', str(root), 'rev-parse', '--show-toplevel'], text=True).strip()
    if Path(actual).resolve() != root or not (root / 'CMakeLists.txt').is_file():
        parser.error('worktree must be an Eternity repository root')
    main = Path(__file__).resolve().parents[1]
    def common(directory):
        return subprocess.check_output(['git', '-C', str(directory), 'rev-parse', '--path-format=absolute', '--git-common-dir'], text=True).strip()
    if Path(common(root)).resolve() != Path(common(main)).resolve():
        parser.error('worktree must share Eternity Git history')
    name = 'eternity-target-' + uuid.uuid4().hex
    command = ['docker', 'run', '--rm', '--name', name, '--mount', f'type=bind,source={root},target=/src',
               'eternity-dev:bookworm', 'sh', '-c',
               'cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug && '
               'cmake --build build -j4 --target "$1" && '
               'ctest --test-dir build --output-on-failure --no-tests=error -R "$2"',
               'targeted', args.target, '^' + re.escape(args.target) + '$']
    with resource_lock(root, 'build', 2):
        try:
            subprocess.run(command, cwd=root, check=True)
        except BaseException:
            subprocess.run(['docker', 'stop', '--timeout', '5', name], stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL, check=False)
            raise


if __name__ == '__main__':
    main()
