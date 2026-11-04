"""Cross-worktree locks for Eternity's shared SQL, Ghidra and build resources."""
from contextlib import contextmanager
import fcntl
import os
from pathlib import Path
import subprocess
import time


@contextmanager
def resource_lock(root: Path, name: str, slots: int = 1):
    configured = os.environ.get('ETERNITY_SHARED_LOCK_DIR')
    if configured:
        directory = Path(configured)
    else:
        common = subprocess.check_output(
            ['git', '-C', str(root), 'rev-parse', '--path-format=absolute', '--git-common-dir'], text=True).strip()
        directory = Path(common) / 'eternity-locks'
    directory.mkdir(parents=True, exist_ok=True)
    handles = [(directory / f'{name}-{i}.lock').open('a+') for i in range(slots)]
    acquired = None
    try:
        announced = False
        while acquired is None:
            for handle in handles:
                try:
                    fcntl.flock(handle, fcntl.LOCK_EX | fcntl.LOCK_NB)
                    acquired = handle
                    break
                except BlockingIOError:
                    pass
            if acquired is None:
                if not announced:
                    print(f'Waiting for shared Eternity resource: {name}', flush=True)
                    announced = True
                time.sleep(0.25)
        yield
    finally:
        if acquired is not None:
            fcntl.flock(acquired, fcntl.LOCK_UN)
        for handle in handles:
            handle.close()
