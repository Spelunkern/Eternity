#!/usr/bin/env python3
"""Scheduler tests; never start real workers, terminals, or coordinator sessions."""
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
import parallel
from parallel_supervisor import Supervisor


class SupervisorTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name) / 'repo'
        self.state = self.root / '.local/parallel'
        self.task = parallel.TASKS[0]
        self.folder = self.state / 'workers' / self.task[0]
        self.folder.mkdir(parents=True)
        self.clock = [1000]
        self.launches = []
        self.notifications = []
        self.patch_tasks = patch.object(parallel, 'TASKS', [self.task])
        self.patch_tasks.start()
        self.addCleanup(self.patch_tasks.stop)
        self.patch_git = patch.object(parallel, 'git', return_value='abc123')
        self.patch_git.start()
        self.addCleanup(self.patch_git.stop)
        self.supervisor = self.make()

    def make(self, live=False):
        return Supervisor(self.root, launcher=lambda task: self.launches.append(task),
                          notifier=lambda text, manifest: self.notifications.append(text) or True,
                          process_alive=lambda pid: live and bool(pid), clock=lambda: self.clock[0])

    def status(self, **kwargs):
        state = dict(state='finished', started='one', ended='two')
        state.update(kwargs)
        parallel.save(self.folder / 'status.json', state)

    def control(self, value):
        parallel.save(self.state / 'control.json', value)

    def test_round_snapshot_and_relaunch_are_durable(self):
        self.status()
        (self.folder / 'last-message.txt').write_text('handoff')
        self.supervisor.tick()
        self.make().tick()
        self.assertEqual(len(self.launches), 1)
        snapshots = list((self.folder / 'rounds').glob('*.json'))
        self.assertEqual(len(snapshots), 1)
        self.assertEqual(json.loads(snapshots[0].read_text())['report'], 'handoff')
        self.assertEqual(len(self.notifications), 1)

    def test_live_worker_is_never_duplicated(self):
        self.status(state='running', pid=123)
        self.make(live=True).tick()
        self.assertEqual(self.launches, [])

    def test_idle_persistent_terminal_gets_one_dispatch(self):
        self.status(pid=123)
        supervisor = self.make(live=True)
        supervisor.tick()
        supervisor.tick()
        self.assertEqual(len(self.launches), 1)

    def test_processed_assignment_does_not_retrigger_wait(self):
        self.control({'workers': {self.task[0]: {'assignment': 'Next block', 'assignment_id': 'job-2'}}})
        self.status(state='running', pid=123, assignment_id='job-2')
        self.make(live=True).tick()
        self.status(pid=123, assignment_id='job-2')
        (self.folder / 'last-message.txt').write_text('WAITING_FOR_ASSIGNMENT')
        self.make(live=True).tick()
        self.assertEqual(self.launches, [])

    def test_pause_and_explicit_completion(self):
        self.status()
        for policy in ({'active': False}, {'objective_complete': True}):
            self.control(policy)
            self.supervisor.tick()
        self.assertEqual(self.launches, [])
        self.control({'active': True})
        self.supervisor.tick()
        self.assertEqual(len(self.launches), 1)

    def test_waiting_report_unblocked_by_assignment(self):
        self.status()
        (self.folder / 'last-message.txt').write_text('WAITING_FOR_ASSIGNMENT')
        self.supervisor.tick()
        self.assertEqual(self.launches, [])
        self.control({'workers': {self.task[0]: {'assignment': 'Next domain', 'assignment_id': 'next'}}})
        self.supervisor.tick()
        self.assertEqual(len(self.launches), 1)

    def test_stop_marker_prevents_launch(self):
        self.status()
        (self.folder / 'stop').touch()
        self.supervisor.tick()
        self.assertEqual(self.launches, [])

    def test_failures_back_off_then_hold(self):
        for attempt in range(3):
            parallel.save(self.folder / 'status.json', {'state': 'failed', 'started': str(attempt)})
            self.supervisor.tick()
            self.clock[0] += 100
            self.supervisor.tick()
        self.assertEqual(len(self.launches), 2)
        self.clock[0] += 99999
        self.supervisor.tick()
        self.assertEqual(len(self.launches), 2)

    def test_auth_failure_holds_without_retry(self):
        parallel.save(self.folder / 'status.json', {'state': 'failed'})
        (self.folder / 'events.jsonl').write_text('authentication failed')
        self.supervisor.tick()
        self.clock[0] += 9999
        self.supervisor.tick()
        self.assertEqual(self.launches, [])

    def test_absent_coordinator_keeps_notification_file(self):
        self.status()
        supervisor = Supervisor(self.root, launcher=lambda task: None, process_alive=lambda pid: False,
                                clock=lambda: self.clock[0])
        supervisor.tick()
        self.assertTrue(supervisor.data['pending'])
        self.assertTrue((self.state / 'coordinator-notification.json').exists())


if __name__ == '__main__':
    unittest.main()
