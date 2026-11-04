#!/usr/bin/env python3
"""Prepare and launch nine isolated Eternity Codex workers in Konsole."""
import argparse
from contextlib import contextmanager
from datetime import datetime, timezone
import fcntl
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import tempfile
import time
import tomllib
import uuid

ROOT = Path(__file__).resolve().parents[1]
TASKS = [
    ('01-mob-damage', 'Mob damage and party credit', 'CMob::Damage 00490cf0/00490ee0 and DamageParty 00490e40/00490e90', 'mob_combat'),
    ('02-mob-drops', 'Mob loot definitions and drops', 'CMob::ItemDrop 004a5ea0/004a65a0 and original MobItems definitions', 'mob_drops'),
    ('03-mob-skills', 'Mob applied skill ownership and cleanup', 'CMob::ClearApplySkill 004a0990, real skill list and shared pool', 'mob_applied_skills'),
    ('04-obelisks', 'Obelisk ownership and respawn', 'CMap::ObeliskDestroy 0042a410 and ObeliskRespawn 0042a6f0', 'obelisk_runtime'),
    ('05-party-state', 'Party experience state and reward range', 'CParty::ChkPartyExpByLevel 00444d80/GetPartyExpByLevel 00444e40 and CUser::EnableApplyRange 0045de60', 'party_runtime'),
    ('06-quest-start', 'Quest admission and start', 'CUser::QuestEnableStart 0047f080, QuestStartRequest 0047f220, QuestStart 0047ea70 and start checks 0047f390..0047f570', 'quest_start'),
    ('07-quest-requests', 'Quest end requests and item requirements', 'CUser::QuestEndRequest 0047f2a0, completion checks 0047f630..0047f740 and QuestFindItem 0047f790/0047f8e0; core QuestEnd/Select belong to coordinator', 'quest_requests'),
    ('08-mob-targeting', 'Mob targeting', 'CMob::SetTarget 0049fad0 and SetTargetEx 004b65e0, exact target/scene transitions', 'mob_targeting'),
    ('09-mob-events', 'Mob FSM event transitions', 'CMob::OnMove/OnMoveEnd/OnAttacked/OnAttackable/OnNormalReset/OnDeath/OnReturnHome 004b60d0..004b6270', 'mob_events'),
]


def run(*args, **kwargs):
    return subprocess.run(list(args), check=True, text=True, **kwargs)


def git(*args, cwd=ROOT):
    return subprocess.check_output(['git', '-C', str(cwd), *args], text=True).strip()


def now():
    return datetime.now(timezone.utc).isoformat()


def save(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile('w', dir=path.parent, prefix=path.name + '.', delete=False) as stream:
        stream.write(json.dumps(value, indent=2) + '\n')
        temporary = Path(stream.name)
    temporary.replace(path)


@contextmanager
def exclusive(path, nonblocking=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a+') as handle:
        fcntl.flock(handle, fcntl.LOCK_EX | (fcntl.LOCK_NB if nonblocking else 0))
        yield handle


def alive(pid):
    if not pid:
        return False
    try:
        pid = int(pid)
        if pid <= 0:
            return False
        os.kill(pid, 0)
        return True
    except (ProcessLookupError, ValueError, TypeError):
        return False
    except PermissionError:
        return True


def configuration(root):
    home = Path(os.environ.get('CODEX_HOME', str(Path.home() / '.codex')))
    config = home / 'config.toml'
    current = tomllib.loads(config.read_text()) if config.exists() else {}
    settings = root / '.local/parallel/settings.json'
    if settings.exists():
        return json.loads(settings.read_text())
    model = current.get('model')
    if not model:
        raise RuntimeError('No configured Codex model; set model in ~/.codex/config.toml first')
    result = {'model': model, 'effort': 'low', 'service_tier': current.get('service_tier'),
              'sandbox': 'danger-full-access', 'approval': 'never'}
    save(settings, result)
    return result


def paths(root, task):
    state = root / '.local/parallel'
    return root.parent / 'Eternity-workers' / task[0], state / 'workers' / task[0]


def control(root):
    path = root / '.local/parallel/control.json'
    return json.loads(path.read_text()) if path.exists() else {'active': True, 'objective_complete': False, 'workers': {}}


def handoff_waiting(text):
    lines = text.strip().splitlines()
    return bool(lines) and lines[-1].strip() == 'WAITING_FOR_ASSIGNMENT'


def continuation_prompt(root, task, status):
    policy = control(root).get('workers', {}).get(task[0], {})
    prefix = policy.get('prefix', task[3])
    assignment = policy.get('assignment')
    scope = f'Coordinator updated your exclusive new file prefix to {prefix}. Previous ownership restrictions on main and other workers still apply.' if prefix != task[3] else f'Your exclusive subsystem/file prefix remains {prefix}.'
    return f'''Continue the SAME Eternity worker context after the previous milestone. The user explicitly authorized continuous work until the complete native server objective is achieved, not stopping permanently after a fixed task.
Read current {root}/CONTINUITY.md and {root}/PARALLEL_WORK.md and review your last commit/report. {scope}
{'NEW COORDINATOR ASSIGNMENT: ' + assignment if assignment else 'Choose the next concrete, highest-value missing behavior or integration evidence within your existing ownership. Use your prior evidence and report; do not repeat completed work. The current subsystem description is: ' + task[2]}
Finish one substantive implementation/evidence block, run meaningful checks, commit to your worker branch and update reports/parallel/{task[0]}.md with completed behavior, commit ID, validation, remaining work and needed integration. Then return a handoff; the supervisor will resume this same context for the next block. No new child agents, no main edits, merges, resets or pushes. Preserve original binaries. Shared-resource locks and original goal constraints remain in force. Never infer global completion from test counts.
If no independent meaningful work is possible inside your ownership, end the handoff with the exact marker WAITING_FOR_ASSIGNMENT and a concrete dependency/next-task proposal. The coordinator will assign a new scope; do not poll by burning model turns or take another worker's files. An account/service failure must be reported rather than retried indefinitely.
Previous runner result: {status.get('state', 'unknown')}; previous exit: {status.get('exit_code')}.
'''


def prompt(root, tree, task):
    identity, title, symbols, stem = task
    return f'''You are Eternity worker {identity}, one of nine independent terminals coordinated by the main Eternity session.
Objective: complete native Linux C++20 reconstruction of the original EP4.5 server, preserving original EP4 behavior. The server is currently not playable. Work on the assigned server subsystem until it is implemented and verified.
Read AGENTS.md, CONTINUITY.md and {root}/PARALLEL_WORK.md first. Main checkout {root} is READ-ONLY to you. Your only source checkout is {tree}; your branch is workers/{identity}. Do not create child agents or additional sessions.
ASSIGNMENT: {title}. Evidence: ps_game.exe {symbols}.
OWNERSHIP: new include/eternity/game/{stem}*.hpp, src/game/{stem}*.cpp, tools/{stem}*_oracle.py, tests/{stem}*_unit.cpp, tests/data/{stem.replace('_','-')}* and reports/parallel/{identity}.md. You may register your own sources/targets in your PRIVATE CMakeLists.txt. Do not edit shared canonical headers, AGENTS.md, CONTINUITY.md, tools/parallel*, tools/dev.py, tools/reverse.py, SQL fixtures or other owners' files. If an existing owner needs a field/interface, describe the exact evidence and required change in your report; use an explicit input projection meanwhile. Never edit files in the main checkout, another worker, or the shared Ghidra project directly. No git reset/clean/force operations, main checkout, merges or pushes. Commit only your scoped work to your branch.
Read-only evidence: {root}/work/reverse/ps_game.exe contains complete exports; other {root}/work subfolders contain later focused Ghidra exports. Preserve original binaries and verify SHA256SUMS. Generated work stays in your ignored work/. Use .local/oracle-env/bin/python for original execution fixtures; write temporary fixtures under work/ first and copy only when no tests read the target. Cite executable/symbol/address. Required world services must not default to no-op; distinguish recovered coordination from missing providers. Do not claim gameplay completion from tests.
SHARED RESOURCES: tools/reverse.py automatically serializes Ghidra. Your work/ghidra is the shared project; always use --process and --output work/exports/{identity} (never --import or alter the project manually). Use JAVA_HOME=/home/franco/.local/share/castaway-tools/jdk-21.0.12.1+1 and --ghidra /home/franco/.local/share/castaway-tools/ghidra_12.1.4_PUBLIC. tools/dev.py automatically limits builds/tests to two simultaneous jobs and serializes isolated SQL integration across all worktrees. Use those tools; do not bypass their locks with raw concurrent full builds or SQL commands. Do not start/reset/restore/provision databases, change schema or expose credentials. SQL lifecycle and final integration belong to the coordinator. Assets and credentials in your .local are private copies; use only Eternity's isolated database if integration is required. Exclude ps_gamewar, Phoenix and client work; SQLite migration is deferred.
Implement actual behavior from evidence, create meaningful original-execution fixtures and native comparisons, build and run relevant tests. Keep code/docs English and progress Spanish. Report missing evidence instead of guessing.
When finished, write reports/parallel/{identity}.md with source evidence, behavior, exact validation results, remaining dependencies, needed canonical interface changes and commit hash. Commit your implementation to your branch. Do not merge or push. Finish with a concise handoff. The coordinator integrates and runs the shared complete checks.
'''


def prepare(root):
    state = root / '.local/parallel'
    state.mkdir(parents=True, exist_ok=True, mode=0o700)
    os.chmod(state, 0o700)
    settings = configuration(root)
    common = Path(git('rev-parse', '--path-format=absolute', '--git-common-dir', cwd=root))
    with exclusive(state / 'prepare.lock'):
        manifest_path = state / 'manifest.json'
        manifest = json.loads(manifest_path.read_text()) if manifest_path.exists() else {'base': git('rev-parse', 'HEAD', cwd=root), 'workers': {}}
        if not manifest.get('coordinator_thread') and os.environ.get('CODEX_THREAD_ID'):
            manifest['coordinator_thread'] = os.environ['CODEX_THREAD_ID']
        if not (root / 'tools/parallel_lock.py').is_file():
            raise RuntimeError('Missing shared lock helper')
        # A worker must receive the committed launcher/locks, never an older base.
        run('git', '-C', str(root), 'cat-file', '-e', f"{manifest['base']}:tools/parallel_lock.py", stdout=subprocess.DEVNULL)
        for task in TASKS:
            tree, folder = paths(root, task)
            branch = f'workers/{task[0]}'
            folder.mkdir(parents=True, exist_ok=True)
            if not tree.exists():
                tree.parent.mkdir(parents=True, exist_ok=True)
                existing = subprocess.run(['git', '-C', str(root), 'show-ref', '--verify', '--quiet', f'refs/heads/{branch}']).returncode == 0
                args = ['git', '-C', str(root), 'worktree', 'add']
                args += [str(tree), branch] if existing else ['-b', branch, str(tree), manifest['base']]
                run(*args)
            if Path(git('rev-parse', '--show-toplevel', cwd=tree)).resolve() != tree.resolve() or git('branch', '--show-current', cwd=tree) != branch:
                raise RuntimeError(f'Unexpected checkout at {tree}; refusing to reuse it')
            local = tree / '.local'
            local.mkdir(exist_ok=True, mode=0o700)
            os.chmod(local, 0o700)
            for name in ('runtime.env', 'sqlserver.env'):
                source, destination = root / '.local' / name, local / name
                if source.is_file() and not destination.exists():
                    shutil.copy2(source, destination)
                    os.chmod(destination, 0o600)
            assets = root / '.local/server-data'
            if assets.is_dir() and not (local / 'server-data').exists():
                shutil.copytree(assets, local / 'server-data')
            oracle = root / '.local/oracle-env'
            if oracle.is_dir() and not (local / 'oracle-env').exists():
                (local / 'oracle-env').symlink_to(oracle, target_is_directory=True)
            work = tree / 'work'
            work.mkdir(exist_ok=True)
            project = root / 'work/ghidra'
            if project.is_dir() and not (work / 'ghidra').exists():
                (work / 'ghidra').symlink_to(project, target_is_directory=True)
            (folder / 'prompt.txt').write_text(prompt(root, tree, task))
            manifest['workers'][task[0]] = {'title': task[1], 'path': str(tree), 'branch': branch, 'folder': str(folder)}
        manifest['settings'] = settings
        manifest['lock_directory'] = str(common / 'eternity-locks')
        save(manifest_path, manifest)
    return manifest


def codex_command(settings, tree, report, thread_id=None):
    executable = settings.get('codex_executable') or shutil.which('codex')
    if not executable:
        home = Path.home()
        candidates = (home / '.local/bin/codex', home / '.codex/packages/standalone/current/bin/codex',
                      home / '.cargo/bin/codex', Path('/usr/local/bin/codex'), Path('/usr/bin/codex'))
        executable = next((str(path) for path in candidates if path.is_file() and os.access(path, os.X_OK)), None)
    if not executable:
        raise RuntimeError('Codex CLI is not on PATH')
    command = [executable, '-m', settings['model'], '-s', settings['sandbox'], '-a', settings['approval'],
               '-c', 'model_reasoning_effort=' + json.dumps(settings['effort'])]
    if settings.get('service_tier'):
        command += ['-c', 'service_tier=' + json.dumps(settings['service_tier'])]
    command += ['-C', str(tree)]
    if thread_id:
        return command + ['exec', 'resume', '--json', '-o', str(report), thread_id, '-']
    return command + ['exec', '--json', '--color', 'never', '-o', str(report), '-']


def worker(root, identity):
    task = next(t for t in TASKS if t[0] == identity)
    tree, folder = paths(root, task)
    state_path = folder / 'status.json'
    manifest = json.loads((root / '.local/parallel/manifest.json').read_text())
    try:
        with exclusive(folder / 'worker.lock', nonblocking=True) as handle:
            if git('branch', '--show-current', cwd=tree) != f'workers/{identity}':
                raise RuntimeError('Worker branch changed; refusing to run')
            previous = json.loads(state_path.read_text()) if state_path.exists() else {}
            policy = control(root).get('workers', {}).get(identity, {})
            status = {'worker': identity, 'state': 'running', 'pid': os.getpid(), 'started': now(),
                      'round': previous.get('round', 0) + 1, 'gui_pid': os.getppid(),
                      'assignment_id': policy.get('assignment_id')}
            if previous.get('thread_id'):
                status['thread_id'] = previous['thread_id']
            save(state_path, status)
            env = os.environ.copy()
            for name in ('CODEX_THREAD_ID', 'CODEX_SESSION_ID', 'CODEX_PARENT_THREAD_ID', 'CODEX_INTERNAL_ORIGINATOR_OVERRIDE'):
                env.pop(name, None)
            env['ETERNITY_SHARED_LOCK_DIR'] = manifest['lock_directory']
            report = folder / 'last-message.txt'
            if report.exists():
                shutil.copy2(report, folder / f'message-round-{previous.get("round", 0)}.txt')
                report.unlink()
            command = codex_command(manifest['settings'], tree, report, status.get('thread_id'))
            env['PATH'] = str(Path(command[0]).parent) + os.pathsep + env.get('PATH', os.defpath)
            print(f'Eternity {identity}: {task[1]}\n{tree}\n' + json.dumps(manifest['settings']), flush=True)
            with (folder / 'events.jsonl').open('a') as log:
                child = subprocess.Popen(command, cwd=tree, env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, text=True, pass_fds=(handle.fileno(),))
                status['codex_pid'] = child.pid
                save(state_path, status)
                instruction = continuation_prompt(root, task, previous) if status.get('thread_id') else (folder / 'prompt.txt').read_text()
                child.stdin.write(instruction)
                child.stdin.close()
                try:
                    for line in child.stdout:
                        log.write(line)
                        log.flush()
                        try:
                            event = json.loads(line)
                        except ValueError:
                            print(line, end='', flush=True)
                            continue
                        if event.get('type') == 'thread.started':
                            status['thread_id'] = event.get('thread_id')
                            save(state_path, status)
                        item = event.get('item', {})
                        message = item.get('text') or item.get('command') or event.get('message')
                        if message:
                            print(message, flush=True)
                        if item.get('aggregated_output'):
                            print(item['aggregated_output'], flush=True)
                    code = child.wait()
                except KeyboardInterrupt:
                    child.terminate()
                    child.wait()
                    code = 130
                finally:
                    child.stdout.close()
            status.update(state='finished' if code == 0 else 'failed', exit_code=code, ended=now())
            status['codex_pid'] = None
            status['waiting'] = report.exists() and handoff_waiting(report.read_text())
            status['commit'] = git('rev-parse', 'HEAD', cwd=tree)
            save(state_path, status)
            print(f'Worker {identity}: {status["state"]}; handoff: {report}', flush=True)
            return code
    except BlockingIOError:
        print(f'{identity} is already running; no duplicate session opened')
        return 0


def worker_service(root, identity):
    _, folder = paths(root, next(t for t in TASKS if t[0] == identity))
    try:
        with exclusive(folder / 'service.lock', nonblocking=True):
            return worker_service_loop(root, identity)
    except BlockingIOError:
        print(f'{identity} already has a persistent terminal service')
        return 0


def worker_service_loop(root, identity):
    task = next(t for t in TASKS if t[0] == identity)
    _, folder = paths(root, task)
    dispatch = folder / 'dispatch.json'
    token = json.loads(dispatch.read_text()).get('id') if dispatch.exists() else None
    while True:
        state = control(root)
        if not state.get('active', True) or state.get('objective_complete', False):
            return 0
        code = worker(root, identity)
        print('Milestone recorded. Waiting for supervisor continuation or coordinator assignment.', flush=True)
        while True:
            state = control(root)
            if not state.get('active', True) or state.get('objective_complete', False):
                return code
            fresh = json.loads(dispatch.read_text()).get('id') if dispatch.exists() else None
            if fresh and fresh != token:
                token = fresh
                break
            time.sleep(2)


def retire_finished_terminal(root, task, status):
    """Close only a proven single-session, finished window created by this runner."""
    if alive(status.get('pid')) or alive(status.get('codex_pid')):
        return
    metadata = root / '.local/parallel/supervisor-status.json'
    if not metadata.exists() or not shutil.which('qdbus6'):
        return
    terminal = json.loads(metadata.read_text()).get('workers', {}).get(task[0], {}).get('terminal', {})
    pid = terminal.get('pid')
    if not pid or not alive(pid):
        return
    try:
        base = Path('/proc') / str(pid)
        stat = (base / 'stat').read_text().rsplit(')', 1)[1].split()
        command = (base / 'cmdline').read_bytes().replace(b'\0', b' ').decode()
        if stat[19] != terminal.get('start') or 'parallel.py' not in command or task[0] not in command:
            return
        service = f'org.kde.konsole-{pid}'
        count = subprocess.check_output(['qdbus6', service, '/Windows/1', 'org.kde.konsole.Window.sessionCount'], text=True, timeout=5).strip()
        current = subprocess.check_output(['qdbus6', service, '/Windows/1', 'org.kde.konsole.Window.currentSession'], text=True, timeout=5).strip()
        child = subprocess.check_output(['qdbus6', service, '/Sessions/' + current, 'org.kde.konsole.Session.processId'], text=True, timeout=5).strip()
        if count == '1' and int(child) in (0, -1, status.get('pid')) and not alive(int(child)):
            os.kill(pid, signal.SIGTERM)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError):
        return


def start_one(root, task, continuation=False):
    tree, folder = paths(root, task)
    status_path = folder / 'status.json'
    with exclusive(folder / 'start.lock'):
        current = control(root)
        policy = current.get('workers', {}).get(task[0], {})
        if not current.get('active', True) or current.get('objective_complete') or policy.get('stop') or policy.get('wait') or policy.get('active') is False:
            return False
        status = json.loads(status_path.read_text()) if status_path.exists() else {}
        if status.get('state') in ('running', 'launching') and (alive(status.get('pid')) or alive(status.get('codex_pid'))):
            return False
        if status.get('state') == 'finished' and not continuation:
            return False
        if alive(status.get('pid')):
            save(folder / 'dispatch.json', {'id': str(uuid.uuid4()), 'created': now()})
            print(f'Continuing {task[0]} in its existing terminal/context')
            return True
        retire_finished_terminal(root, task, status)
        command = ['konsole', '--separate', '--hold', '--workdir', str(tree),
                   '-p', f'LocalTabTitleFormat=Eternity {task[0]}', '-e', sys.executable,
                   str(root / 'tools/parallel.py'), '--root', str(root), 'worker', task[0]]
        # Preserve previous thread/round for exact-context resume.
        status.update(worker=task[0], state='launching', pid=None, codex_pid=None, launched=now())
        save(status_path, status)
        with (folder / 'terminal.log').open('a') as log:
            process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        latest = json.loads(status_path.read_text())
        if latest.get('state') == 'launching':
            latest['pid'] = process.pid
            save(status_path, latest)
        print(f'Opened {task[0]}: {task[1]}')
        return True


def start_supervisor(root):
    state = root / '.local/parallel'
    with exclusive(state / 'supervisor-launch.lock'):
        marker = state / 'supervisor-process.json'
        status = json.loads(marker.read_text()) if marker.exists() else {}
        if alive(status.get('pid')):
            try:
                command = (Path('/proc') / str(status['pid']) / 'cmdline').read_bytes()
                if b'parallel_supervisor.py' in command:
                    return
            except OSError:
                pass
        with (state / 'supervisor.log').open('a') as log:
            child = subprocess.Popen([sys.executable, str(root / 'tools/parallel_supervisor.py'), '--root', str(root)],
                                     cwd=root, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
        save(marker, {'pid': child.pid, 'started': now()})


def launch(root):
    if not shutil.which('konsole'):
        raise RuntimeError('Konsole is required for the nine terminal windows')
    if not (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        raise RuntimeError('Run the desktop launcher inside the graphical desktop session')
    prepare(root)
    state = root / '.local/parallel'
    policy = control(root)
    if policy.get('objective_complete'):
        print('Objective is marked complete; no workers restarted')
        return
    policy['active'] = True
    save(state / 'control.json', policy)
    with exclusive(state / 'launch.lock'):
        for task in TASKS:
            start_one(root, task, continuation=True)
    start_supervisor(root)
    print('Nine continuous worker terminals plus the coordinator. Run: python3 tools/parallel.py status')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    sub = parser.add_subparsers(dest='action', required=True)
    for action in ('prepare', 'launch', 'status', 'plan', 'supervise', 'stop'):
        sub.add_parser(action)
    assignment = sub.add_parser('assign')
    assignment.add_argument('identity', choices=[t[0] for t in TASKS])
    assignment.add_argument('message')
    assignment.add_argument('--prefix')
    command = sub.add_parser('worker')
    command.add_argument('identity', choices=[t[0] for t in TASKS])
    args = parser.parse_args()
    root = args.root.resolve()
    if args.action == 'prepare':
        manifest = prepare(root)
        print(json.dumps(manifest, indent=2))
    elif args.action == 'launch':
        launch(root)
    elif args.action == 'worker':
        return worker_service(root, args.identity)
    elif args.action == 'supervise':
        prepare(root)
        start_supervisor(root)
    elif args.action == 'stop':
        with exclusive(root / '.local/parallel/control.lock'):
            state = control(root)
            state['active'] = False
            save(root / '.local/parallel/control.json', state)
        print('Continuous scheduling stopped; active blocks finish and preserve their state.')
    elif args.action == 'assign':
        if args.prefix and (not args.prefix.replace('_', '').isalnum() or not args.prefix[0].isalpha()):
            raise RuntimeError('Assignment prefix must be a simple identifier')
        with exclusive(root / '.local/parallel/control.lock'):
            state = control(root)
            workers = state.setdefault('workers', {})
            for task in TASKS:
                if task[0] != args.identity and args.prefix and args.prefix in [task[3], *workers.get(task[0], {}).get('owned_prefixes', [])]:
                    raise RuntimeError('That prefix belongs to another worker')
            policy = workers.setdefault(args.identity, {})
            if args.prefix:
                policy['prefix'] = args.prefix
                policy.setdefault('owned_prefixes', []).append(args.prefix)
            policy.update(assignment=args.message, assignment_id=str(uuid.uuid4()), wait=False, stop=False, active=True)
            save(root / '.local/parallel/control.json', state)
        start_supervisor(root)
        print(f'Queued adaptive assignment for {args.identity}')
    elif args.action == 'plan':
        print(json.dumps({'settings': configuration(root), 'tasks': TASKS}, indent=2))
    else:
        for task in TASKS:
            tree, folder = paths(root, task)
            path = folder / 'status.json'
            status = json.loads(path.read_text()) if path.exists() else {'state': 'not-started'}
            if status['state'] in ('running', 'launching') and not (alive(status.get('pid')) or alive(status.get('codex_pid'))):
                status['state'] = 'stopped'
            if status['state'] == 'finished' and alive(status.get('pid')):
                status['state'] = 'waiting' if status.get('waiting') else 'next-block'
            print(f'{task[0]:22} {status["state"]:12} {status.get("thread_id", "")} {tree}')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        print(f'Eternity parallel launcher: {error}', file=sys.stderr)
        sys.exit(1)
