#!/usr/bin/env python3
"""Exercise isolation, idempotence and CLI inheritance without starting model calls."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1] / 'tools'
sys.path.insert(0, str(TOOLS))
import parallel
from parallel_lock import resource_lock


class ParallelTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name) / 'Eternity'
        (self.root / 'tools').mkdir(parents=True)
        for name in ('parallel.py', 'parallel_lock.py'):
            (self.root / 'tools' / name).write_text((TOOLS / name).read_text())
        (self.root / 'AGENTS.md').write_text('Test fixture\n')
        (self.root / 'CONTINUITY.md').write_text('Test fixture\n')
        (self.root / '.gitignore').write_text('.local/\nwork/\n')
        for arguments in (('init', '-q'), ('config', 'user.email', 'test@example.invalid'),
                          ('config', 'user.name', 'Eternity test'), ('add', '.'), ('commit', '-qm', 'Fixture')):
            subprocess.run(['git', '-C', str(self.root), *arguments], check=True, capture_output=True)
        parallel.save(self.root / '.local/parallel/settings.json', {
            'model': 'gpt-6.1-sol', 'effort': 'low', 'service_tier': 'fast',
            'sandbox': 'danger-full-access', 'approval': 'never'})

    def tearDown(self):
        self.temporary.cleanup()

    def test_nine_isolated_idempotent_worktrees(self):
        first = parallel.prepare(self.root)
        second = parallel.prepare(self.root)
        self.assertEqual(first, second)
        self.assertEqual(len(first['workers']), 9)
        for identity, worker in first['workers'].items():
            tree = Path(worker['path'])
            self.assertNotEqual(tree, self.root)
            self.assertEqual(parallel.git('branch', '--show-current', cwd=tree), f'workers/{identity}')
            (tree / 'private.txt').write_text(identity)
        self.assertFalse((self.root / 'private.txt').exists())
        for identity, worker in first['workers'].items():
            self.assertEqual((Path(worker['path']) / 'private.txt').read_text(), identity)

    def test_worker_settings_environment_and_duplicate_lock(self):
        manifest = parallel.prepare(self.root)
        task = parallel.TASKS[0]
        tree, folder = parallel.paths(self.root, task)
        fake_bin = self.root / '.local/bin'
        fake_bin.mkdir()
        fake = fake_bin / 'codex'
        captured = folder / 'fake-arguments.json'
        fake.write_text('#!/usr/bin/env python3\nimport json,os,sys\n'
                        'from pathlib import Path\n'
                        'sys.stdin.read()\n'
                        'Path(os.environ["FAKE_CAPTURE"]).write_text(json.dumps({"args":sys.argv[1:],"thread":os.environ.get("CODEX_THREAD_ID"),"session":os.environ.get("CODEX_SESSION_ID")}))\n'
                        'print(json.dumps({"type":"thread.started","thread_id":"test-worker"}))\n'
                        'print(json.dumps({"type":"item.completed","item":{"text":"Finished fixture"}}))\n')
        fake.chmod(0o755)
        environment = {'PATH': str(fake_bin) + os.pathsep + os.environ['PATH'],
                       'FAKE_CAPTURE': str(captured), 'CODEX_THREAD_ID': 'parent-thread', 'CODEX_SESSION_ID': 'parent-session'}
        with patch.dict(os.environ, environment):
            with parallel.exclusive(folder / 'worker.lock'):
                self.assertEqual(parallel.worker(self.root, task[0]), 0)
                self.assertFalse(captured.exists())
            self.assertEqual(parallel.worker(self.root, task[0]), 0)
        data = json.loads(captured.read_text())
        self.assertIsNone(data['thread'])
        self.assertIsNone(data['session'])
        for value in ('gpt-6.1-sol', 'danger-full-access', 'never', 'model_reasoning_effort="low"', 'service_tier="fast"', '--json'):
            self.assertIn(value, data['args'])
        status = json.loads((folder / 'status.json').read_text())
        self.assertEqual(status['state'], 'finished')
        self.assertEqual(status['thread_id'], 'test-worker')

    def test_shared_resource_exclusion_and_release(self):
        directory = self.root / '.local/locks'
        with patch.dict(os.environ, {'ETERNITY_SHARED_LOCK_DIR': str(directory)}):
            with resource_lock(self.root, 'sql'):
                with self.assertRaises(BlockingIOError):
                    with parallel.exclusive(directory / 'sql-0.lock', nonblocking=True):
                        pass
            with parallel.exclusive(directory / 'sql-0.lock', nonblocking=True):
                pass
            with resource_lock(self.root, 'build', 2):
                with resource_lock(self.root, 'build', 2):
                    for index in range(2):
                        with self.assertRaises(BlockingIOError):
                            with parallel.exclusive(directory / f'build-{index}.lock', nonblocking=True):
                                pass


if __name__ == '__main__':
    unittest.main()
