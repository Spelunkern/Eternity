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

    def test_desktop_path_finds_user_codex(self):
        executable = self.root / '.local/bin/codex'
        executable.parent.mkdir(parents=True)
        executable.write_text('#!/bin/sh\nexit 0\n')
        executable.chmod(0o755)
        settings = {'model': 'gpt-6.1-sol', 'effort': 'low', 'sandbox': 'danger-full-access', 'approval': 'never'}
        with patch('parallel.shutil.which', return_value=None), patch('parallel.Path.home', return_value=self.root):
            command = parallel.codex_command(settings, self.root, self.root / 'report.txt')
        self.assertEqual(command[0], str(executable))

    def test_resume_preserves_context_and_permissions(self):
        settings = {'model': 'gpt-6.1-sol', 'effort': 'low', 'sandbox': 'danger-full-access', 'approval': 'never', 'codex_executable': '/fake/codex'}
        command = parallel.codex_command(settings, self.root, self.root / 'report.txt', 'existing-thread')
        self.assertEqual(command[-7:], ['exec', 'resume', '--json', '-o', str(self.root / 'report.txt'), 'existing-thread', '-'])
        self.assertIn('model_reasoning_effort="low"', command)
        self.assertIn('danger-full-access', command)
        self.assertIn('never', command)

    def test_idle_service_dispatch_does_not_open_another_terminal(self):
        parallel.prepare(self.root)
        task = parallel.TASKS[0]
        _, folder = parallel.paths(self.root, task)
        parallel.save(folder / 'status.json', {'state': 'finished', 'pid': os.getpid(), 'thread_id': 'existing-thread'})
        with patch('parallel.subprocess.Popen') as process:
            self.assertTrue(parallel.start_one(self.root, task, continuation=True))
            process.assert_not_called()
        self.assertTrue((folder / 'dispatch.json').exists())
        self.assertFalse(parallel.alive(-1))

    def test_terminal_service_handles_successive_blocks(self):
        parallel.prepare(self.root)
        task = parallel.TASKS[0]
        _, folder = parallel.paths(self.root, task)
        calls = []
        def block(root, identity):
            calls.append(identity)
            return 0
        def sleep(seconds):
            parallel.save(folder / 'dispatch.json', {'id': 'next-block'})
        with patch('parallel.worker', side_effect=block), patch('parallel.time.sleep', side_effect=sleep), patch('parallel.control', side_effect=lambda root: {'active': len(calls)<2}):
            self.assertEqual(parallel.worker_service_loop(self.root, task[0]), 0)
        self.assertEqual(calls, [task[0], task[0]])


if __name__ == '__main__':
    unittest.main()
