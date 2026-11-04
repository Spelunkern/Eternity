#!/usr/bin/env python3
"""Durable non-model scheduler for the nine Eternity worker terminals."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

import parallel


def read(path, default=None):
    try:
        return json.loads(path.read_text())
    except (OSError, ValueError):
        return {} if default is None else default


def process_info(pid):
    """Read identity before trusting a PID, which can be reused."""
    try:
        base = Path('/proc') / str(int(pid))
        stat = (base / 'stat').read_text().rsplit(')', 1)[1].split()
        return {'pid': int(pid), 'ppid': int(stat[1]), 'start': stat[19],
                'command': (base / 'cmdline').read_bytes().replace(b'\0', b' ').decode(errors='replace')}
    except (OSError, ValueError, IndexError, TypeError):
        return None


class Supervisor:
    def __init__(self, root, *, launcher=None, notifier=None, process_alive=None, clock=None):
        self.root = Path(root)
        self.state = self.root / '.local/parallel'
        self.launcher = launcher or (lambda task: parallel.start_one(self.root, task, continuation=True))
        self.notifier = notifier or self.notify
        self.alive = process_alive or parallel.alive
        self.clock = clock or time.time
        self.path = self.state / 'supervisor-status.json'
        self.data = read(self.path)
        self.data.setdefault('workers', {})
        self.data.setdefault('pending', [])

    def notify(self, message, manifest):
        thread = manifest.get('coordinator_thread')
        executable = manifest.get('settings', {}).get('codex_executable') or shutil.which('codex')
        if not thread or not executable:
            return False
        command = [str(Path(executable).resolve()), 'queue', '--thread', thread,
                   '--message', message, '--resume']
        result = subprocess.run(command, text=True, capture_output=True, timeout=30)
        # Success only means accepted for delivery; an active writer may defer it.
        self.data['delivery'] = {'returncode': result.returncode,
                                 'output': (result.stdout + result.stderr)[-2000:]}
        return result.returncode == 0

    def persist(self):
        parallel.save(self.path, self.data)

    def capture_terminal(self, status, record, identity):
        wrapper = process_info(status.get('pid'))
        if not wrapper or 'parallel.py' not in wrapper['command'] or identity not in wrapper['command']:
            return
        parent = process_info(wrapper['ppid'])
        if parent and Path(parent['command'].split(' ', 1)[0]).name == 'konsole':
            record['terminal'] = parent
        # Deliberately retain held windows. Closing a GUI safely requires ownership
        # of every session, which a parent PID alone cannot establish.
        if record.get('terminal'):
            record['terminal_cleanup'] = 'preserved: session ownership cannot be proven'

    def tick(self):
        current = self.clock()
        manifest = read(self.state / 'manifest.json')
        control = read(self.state / 'control.json')
        enabled = control.get('active', True) and not control.get('objective_complete', False)
        enabled = enabled and not any((self.state / marker).exists() for marker in ('stop', 'wait', 'STOP', 'WAIT'))
        for task in parallel.TASKS:
            identity = task[0]
            tree, folder = parallel.paths(self.root, task)
            status = read(folder / 'status.json')
            record = self.data['workers'].setdefault(identity, {})
            policy = control.get('workers', {}).get(identity, {})
            assignment = control.get('assignments', {}).get(identity, policy.get('assignment'))
            assignment_key = json.dumps([policy.get('assignment_id'), assignment], sort_keys=True)
            if policy.get('assignment_id') and status.get('assignment_id') == policy['assignment_id']:
                record['assignment'] = assignment_key
            changed_assignment = assignment is not None and assignment_key != record.get('assignment')
            if changed_assignment:
                if record.get('reset_assignment') != assignment_key:
                    record.update(reset_assignment=assignment_key, failures=0, retry_at=0)
            live = status.get('state') in ('running', 'launching') and (self.alive(status.get('pid')) or self.alive(status.get('codex_pid')))
            if live:
                if policy.get('assignment_id') and status.get('assignment_id') == policy.get('assignment_id'):
                    record['assignment'] = assignment_key
                self.capture_terminal(status, record, identity)
                record.pop('dead_since', None)
                continue
            state = status.get('state')
            if state in ('running', 'launching'):
                dead_since = record.setdefault('dead_since', current)
                if current - dead_since < 30:
                    continue
                status = dict(status, state='failed', supervisor_reason='wrapper and Codex processes disappeared')
                state = 'failed'
            if state not in ('finished', 'failed', 'waiting', 'stopped'):
                continue
            token = hashlib.sha256(json.dumps(status, sort_keys=True).encode()).hexdigest()[:24]
            if record.get('round') != token:
                report = folder / 'last-message.txt'
                try:
                    head = parallel.git('rev-parse', 'HEAD', cwd=tree)
                except (OSError, subprocess.CalledProcessError):
                    head = None
                snapshot = {'status': status, 'head': head, 'captured': current,
                            'report': report.read_text() if report.exists() else ''}
                parallel.save(folder / 'rounds' / (token + '.json'), snapshot)
                record.update(round=token, launched=False, waiting_report=parallel.handoff_waiting(snapshot['report']))
                if state == 'failed':
                    record['failures'] = record.get('failures', 0) + 1
                    record['retry_at'] = current + min(900, 30 * 2 ** min(record['failures'] - 1, 5))
                    evidence = snapshot['report'][-4000:]
                    events = folder / 'events.jsonl'
                    if events.exists():
                        with events.open('rb') as stream:
                            stream.seek(max(0, events.stat().st_size - 8000))
                            evidence += stream.read().decode(errors='replace')
                    if any(word in evidence.lower() for word in ('quota', 'unauthorized', 'authentication', 'usage limit', 'insufficient_quota')):
                        record['failures'] = max(3, record['failures'])
                else:
                    record['failures'] = 0
                self.data['pending'].append(f'{identity}: {state}; round {token}; HEAD {head}; snapshot {folder / "rounds" / (token + ".json")}')
                self.persist()
            blocked = policy.get('stop') or policy.get('wait') or policy.get('active') is False
            blocked = blocked or any((folder / marker).exists() for marker in ('stop', 'wait', 'STOP', 'WAIT'))
            waiting = state in ('waiting', 'stopped') or status.get('wait') or status.get('waiting') or record.get('waiting_report')
            allowed = enabled and not blocked and (not waiting or changed_assignment)
            if not allowed or record.get('launched') or record.get('failures', 0) >= 3 or current < record.get('retry_at', 0):
                continue
            # start_one owns the per-worker launch lock and rechecks live writers.
            try:
                launched = self.launcher(task)
                if launched is False:
                    continue
                record.update(launched=True, relaunched_at=current, assignment=assignment_key)
            except Exception as error:
                record['launch_error'] = str(error)
                record['failures'] = record.get('failures', 0) + 1
                record['retry_at'] = current + 60
            self.persist()
        if self.data['pending'] and current >= self.data.get('notify_at', 0):
            message = 'Eternity continuous worker handoffs (server objective remains open):\n' + '\n'.join(self.data['pending'])
            parallel.save(self.state / 'coordinator-notification.json', {'message': message, 'created': current,
                          'thread': manifest.get('coordinator_thread'), 'delivery': 'pending'})
            try:
                delivered = self.notifier(message, manifest)
            except Exception as error:
                delivered = False
                self.data['notification_error'] = str(error)
            if delivered:
                self.data['pending'] = []
            self.data['notify_at'] = current + 30
        self.data.update(pid=os.getpid(), updated=current, active=bool(enabled))
        self.persist()
        return self.data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=parallel.ROOT)
    parser.add_argument('--once', action='store_true')
    args = parser.parse_args()
    try:
        with parallel.exclusive(args.root / '.local/parallel/supervisor.lock', nonblocking=True):
            supervisor = Supervisor(args.root.resolve())
            while True:
                try:
                    supervisor.tick()
                except Exception as error:
                    supervisor.data['scan_error'] = str(error)
                    supervisor.persist()
                if args.once:
                    break
                time.sleep(5)
    except BlockingIOError:
        return 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
