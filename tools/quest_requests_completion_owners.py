#!/usr/bin/env python3
"""Snapshot coordinator-owned canonical owners read-only for worker composition tests."""
import hashlib
from pathlib import Path
root=Path(__file__).resolve().parents[1]
source=Path(__file__).resolve().parents[1]
target=root/'work/quest-requests-completion-owners'
for name in ('quest_runtime','quest_rewards'):
    for relative in (f'include/eternity/game/{name}.hpp',f'src/game/{name}.cpp'):
        data=(source/relative).read_bytes()
        path=target/relative;path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(data)
        print(hashlib.sha256(data).hexdigest(),relative)
