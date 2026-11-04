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
import subprocess
import sys
import tempfile
import tomllib

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
        os.kill(int(pid), 0)
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


def codex_command(settings, tree, report):
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
    return command + ['exec', '--json', '--color', 'never', '-C', str(tree), '-o', str(report), '-']


def worker(root, identity):
    task = next(t for t in TASKS if t[0] == identity)
    tree, folder = paths(root, task)
    state_path = folder / 'status.json'
    manifest = json.loads((root / '.local/parallel/manifest.json').read_text())
    try:
        with exclusive(folder / 'worker.lock', nonblocking=True) as handle:
            if git('branch', '--show-current', cwd=tree) != f'workers/{identity}':
                raise RuntimeError('Worker branch changed; refusing to run')
            status = {'worker': identity, 'state': 'running', 'pid': os.getpid(), 'started': now()}
            save(state_path, status)
            env = os.environ.copy()
            for name in ('CODEX_THREAD_ID', 'CODEX_SESSION_ID', 'CODEX_PARENT_THREAD_ID', 'CODEX_INTERNAL_ORIGINATOR_OVERRIDE'):
                env.pop(name, None)
            env['ETERNITY_SHARED_LOCK_DIR'] = manifest['lock_directory']
            report = folder / 'last-message.txt'
            command = codex_command(manifest['settings'], tree, report)
            env['PATH'] = str(Path(command[0]).parent) + os.pathsep + env.get('PATH', os.defpath)
            print(f'Eternity {identity}: {task[1]}\n{tree}\n' + json.dumps(manifest['settings']), flush=True)
            with (folder / 'events.jsonl').open('a') as log:
                child = subprocess.Popen(command, cwd=tree, env=env, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                         stderr=subprocess.STDOUT, text=True, pass_fds=(handle.fileno(),))
                status['codex_pid'] = child.pid
                save(state_path, status)
                child.stdin.write((folder / 'prompt.txt').read_text())
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
            save(state_path, status)
            print(f'Worker {identity}: {status["state"]}; handoff: {report}', flush=True)
            return code
    except BlockingIOError:
        print(f'{identity} is already running; no duplicate session opened')
        return 0


def launch(root):
    if not shutil.which('konsole'):
        raise RuntimeError('Konsole is required for the nine terminal windows')
    if not (os.environ.get('DISPLAY') or os.environ.get('WAYLAND_DISPLAY')):
        raise RuntimeError('Run the desktop launcher inside the graphical desktop session')
    manifest = prepare(root)
    state = root / '.local/parallel'
    with exclusive(state / 'launch.lock'):
        for task in TASKS:
            tree, folder = paths(root, task)
            status_path = folder / 'status.json'
            status = json.loads(status_path.read_text()) if status_path.exists() else {}
            if alive(status.get('pid')) or alive(status.get('codex_pid')):
                print(f'{task[0]} already active; skipped')
                continue
            if status.get('state') == 'finished':
                print(f'{task[0]} finished; handoff retained, not relaunched')
                continue
            command = ['konsole', '--separate', '--hold', '--workdir', str(tree),
                       '-p', f'LocalTabTitleFormat=Eternity {task[0]}', '-e', sys.executable,
                       str(root / 'tools/parallel.py'), '--root', str(root), 'worker', task[0]]
            save(status_path, {'worker': task[0], 'state': 'launching', 'started': now()})
            with (folder / 'terminal.log').open('a') as log:
                process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, start_new_session=True)
            latest = json.loads(status_path.read_text())
            if latest.get('state') == 'launching':
                latest['pid'] = process.pid
                save(status_path, latest)
            print(f'Opened {task[0]}: {task[1]}')
    print('Nine worker terminals plus the existing coordinator. Run: python3 tools/parallel.py status')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=ROOT)
    sub = parser.add_subparsers(dest='action', required=True)
    for action in ('prepare', 'launch', 'status', 'plan'):
        sub.add_parser(action)
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
        return worker(root, args.identity)
    elif args.action == 'plan':
        print(json.dumps({'settings': configuration(root), 'tasks': TASKS}, indent=2))
    else:
        for task in TASKS:
            tree, folder = paths(root, task)
            path = folder / 'status.json'
            status = json.loads(path.read_text()) if path.exists() else {'state': 'not-started'}
            if status['state'] in ('running', 'launching') and not (alive(status.get('pid')) or alive(status.get('codex_pid'))):
                status['state'] = 'stopped'
            print(f'{task[0]:22} {status["state"]:12} {status.get("thread_id", "")} {tree}')
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError, OSError) as error:
        print(f'Eternity parallel launcher: {error}', file=sys.stderr)
        sys.exit(1)
