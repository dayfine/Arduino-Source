#!/usr/bin/env python3
"""Build and run one macOS development lab, holding its console lock until exit.

Configure once using lab/DEV_INSTANCE.md, then from any directory:
  python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py A
  python3 ~/Projects/Arduino-Source-agents/lab/scripts/start_dev.py B --plan
  python3 lab/scripts/start_dev.py A --config /path/to/dev-labs.json

This opens the GUI, not an automation program. The selected code branch must
include the profile-preparation and native device-ownership guards (#236/#237).
"""

import argparse
from contextlib import contextmanager
from datetime import datetime
import fcntl
import json
import os
from pathlib import Path
import re
import secrets
import shlex
import shutil
import signal
import subprocess
import sys
import time


DEFAULT_CONFIG = Path.home() / '.config/pokemonautomation/dev-labs.json'
# A known revision containing the actual runtime adapters, not just a package flag.
DEVICE_GUARD_REVISION = '45afc7eb048b638f34a62e79c8bbf7d65a377e1e'


def load_config(path, console):
    """Resolve a reviewed slot configuration; never derive device assignments."""
    data = json.loads(Path(path).expanduser().read_text())
    if set(data) != {'schema', 'slots'} or data['schema'] != 1:
        raise ValueError('Expected schema 1 and slots in the configuration.')
    slots = data['slots']
    if not isinstance(slots, dict) or not slots or set(slots) - {'A', 'B'}:
        raise ValueError('Configure only console slots A and B.')
    required = {'worktree', 'branch', 'build_dir', 'stage_root', 'bundle_id', 'signing_identity'}
    all_paths = []
    identifiers = set()
    for name, raw in slots.items():
        if not isinstance(raw, dict) or not required <= raw.keys() or raw.keys() - required - {'cmake_args'}:
            raise ValueError('Each slot requires worktree, branch, build_dir, stage_root, bundle_id and signing_identity.')
        if not all(isinstance(raw[key], str) and raw[key].strip() for key in required):
            raise ValueError('Slot values must be nonempty strings.')
        if not re.fullmatch(r'lab/[A-Za-z0-9_./-]+', raw['branch']) or '..' in raw['branch']:
            raise ValueError('Select a lab/... integration branch, not a feature PR branch.')
        if not re.fullmatch(r'com\.dayfine\.pokemonautomation\.lab[A-Za-z0-9_-]+', raw['bundle_id']):
            raise ValueError('Use a dedicated com.dayfine.pokemonautomation.lab... bundle ID.')
        if raw['bundle_id'] in identifiers or raw['signing_identity'] == '-':
            raise ValueError('Bundle IDs must be distinct and signing must use a persistent certificate.')
        identifiers.add(raw['bundle_id'])
        arguments = raw.get('cmake_args', [])
        if not isinstance(arguments, list) or not all(isinstance(arg, str) for arg in arguments):
            raise ValueError('cmake_args must be an array of strings.')
        # Only cache definitions are accepted: no alternate source/build/preset/toolchain/script.
        if any(not arg.startswith('-D') or '=' not in arg or any(key in arg for key in
               ('PA_LAB_PROFILE', 'CMAKE_HOME_DIRECTORY', 'CMAKE_TOOLCHAIN_FILE', 'CMAKE_PROJECT_',
                'CMAKE_USER_MAKE_RULES_OVERRIDE', 'CMAKE_CXX_COMPILER_LAUNCHER')) for arg in arguments):
            raise ValueError('cmake_args accepts cache -DKEY=VALUE entries; protected build settings cannot be overridden.')
        for key in ('worktree', 'build_dir', 'stage_root'):
            candidate = Path(raw[key]).expanduser()
            if not candidate.is_absolute():
                raise ValueError('All configured paths must be absolute (or start with ~/).')
            candidate = candidate.resolve()
            if candidate == Path.home() or candidate == Path('/'):
                raise ValueError('Select dedicated worktree/build/staging directories.')
            for previous in all_paths:
                if candidate == previous or candidate in previous.parents or previous in candidate.parents:
                    raise ValueError('Worktree, build and staging paths must be separate across all slots.')
            all_paths.append(candidate)
            raw[key] = str(candidate)
    if console not in slots:
        raise ValueError('No configuration for console ' + console)
    return dict(slots[console], console=console, profile='lab-' + console)


@contextmanager
def console_lock(console, root):
    """Publish the existing lablock.sh protocol; never evict an existing owner."""
    lock = Path(root) / 'locks' / console
    lock.parent.mkdir(parents=True, exist_ok=True)
    try:
        lock.mkdir()
    except FileExistsError:
        raise ValueError(f'Console {console} is locked. Use lab/control/lablock.sh -c {console} status; do not delete a live lock.') from None
    token = secrets.token_hex(24)
    try:
        (lock / 'pid').write_text(str(os.getpid()))
        (lock / 'token').write_text(token)
        (lock / 'name').write_text('human-dev-' + console)
        (lock / 'since').write_text(datetime.now().isoformat(timespec='seconds'))
        yield token
    finally:
        if (lock / 'token').is_file() and (lock / 'token').read_text() == token:
            shutil.rmtree(lock)


@contextmanager
def build_lock(root):
    """Only one launcher may configure/build/stage locally at a time."""
    path = Path(root) / 'locks' / 'dev-build.lock'
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open('a') as stream:
        try:
            fcntl.flock(stream, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            raise ValueError('Another dev launcher is building; retry when that build finishes.') from None
        yield


def run(command, *, cwd=None, env=None, capture=False):
    """Supervise a process group, retaining locks through interruption and teardown."""
    print('+ ' + shlex.join(map(str, command)), flush=True)
    child = None
    interrupted_at = None
    previous = {}

    def stop_child(signum, frame):
        nonlocal interrupted_at
        if interrupted_at is None:
            interrupted_at = time.monotonic()
        if child is not None:
            try:
                os.killpg(child.pid, signal.SIGTERM)
            except ProcessLookupError:
                pass

    for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
        previous[signum] = signal.signal(signum, stop_child)
    try:
        child = subprocess.Popen(command, cwd=cwd, env=env, start_new_session=True,
                                 stdout=subprocess.PIPE if capture else None, text=True)
        if interrupted_at is not None:
            stop_child(signal.SIGTERM, None)
        while True:
            try:
                output, _ = child.communicate(timeout=0.25)
                break
            except subprocess.TimeoutExpired:
                if interrupted_at is not None and time.monotonic() - interrupted_at >= 10:
                    try:
                        os.killpg(child.pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
        if interrupted_at is not None:
            raise KeyboardInterrupt
        if child.returncode:
            raise subprocess.CalledProcessError(child.returncode, command)
        return output.strip() if capture else None
    finally:
        # Unexpected errors must not leave an unsupervised GUI after releasing ownership.
        if child is not None and child.poll() is None:
            try:
                os.killpg(child.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            child.communicate()
        for signum, handler in previous.items():
            signal.signal(signum, handler)


def git(slot, *arguments, runner=run):
    return runner(['git', '-C', slot['worktree'], *arguments], capture=True)


def prepare_source(slot, runner=run):
    """Switch only the configured clean lab worktree; update by fast-forward only."""
    worktree = Path(slot['worktree']).resolve()
    actual = git(slot, 'rev-parse', '--show-toplevel', runner=runner)
    if Path(actual).resolve() != worktree:
        raise ValueError('Configured worktree must be the root of its own checkout.')
    if git(slot, 'status', '--porcelain', '--untracked-files=all', runner=runner):
        raise ValueError('Worktree is dirty; commit or preserve changes before starting.')
    runner(['git', '-C', str(worktree), 'fetch', 'origin'])
    runner(['git', '-C', str(worktree), 'switch', slot['branch']])
    remote = 'refs/remotes/origin/' + slot['branch']
    try:
        git(slot, 'show-ref', '--verify', '--quiet', remote, runner=runner)
    except subprocess.CalledProcessError as error:
        if error.returncode != 1:
            raise
        print('No matching remote lab branch; preserving the configured local integration branch.', flush=True)
    else:
        runner(['git', '-C', str(worktree), 'merge', '--ff-only', 'origin/' + slot['branch']])
    # A marker or manifest field cannot prove native device enforcement is compiled.
    git(slot, 'merge-base', '--is-ancestor', DEVICE_GUARD_REVISION, 'HEAD', runner=runner)
    git(slot, 'merge-base', '--is-ancestor', 'origin/lab/main', 'HEAD', runner=runner)
    tool = worktree / 'devtools/lab/lab_instance.py'
    if not tool.is_file():
        raise ValueError('Selected branch lacks the instance-preparation tool (#236).')
    return git(slot, 'rev-parse', 'HEAD', runner=runner)


def validate_profile(slot):
    """Require deliberate setup; do not clone default settings or approve devices."""
    profile = Path.home() / 'Library/Application Support/SerialPrograms/Profiles' / slot['profile']
    policy = json.loads((profile / 'lab-devices.json').read_text())
    if not isinstance(policy, dict):
        raise ValueError('lab-devices.json must be an object.')
    if policy.get('profile') != slot['profile'] or policy.get('schema') != 1:
        raise ValueError('Missing/mismatched lab-devices.json; follow DEVICE_OWNERSHIP.md first.')
    return profile


def check_build_cache(slot):
    """Do not repurpose another checkout's or another profile's cached build."""
    cache = Path(slot['build_dir']) / 'CMakeCache.txt'
    if not cache.exists():
        return
    values = {}
    for line in cache.read_text().splitlines():
        if '=' in line and ':' in line.split('=', 1)[0]:
            key, value = line.split('=', 1)
            values[key.split(':', 1)[0]] = value
    expected_source = Path(slot['worktree']) / 'SerialPrograms'
    if Path(values.get('CMAKE_HOME_DIRECTORY', '/')).resolve() != expected_source:
        raise ValueError('Build cache belongs to a different source checkout.')
    if values.get('PA_LAB_PROFILE') != slot['profile']:
        raise ValueError('Build cache is not bound to this lab profile; use a dedicated build directory.')


def copy_runtime_data(build, stage):
    """macOS Main.cpp resolves runtime data beside the .app, outside its signature."""
    for name in ('Resources', 'Firmware', 'Scripts'):
        source = Path(build) / name
        if not source.is_dir():
            raise ValueError('Build is missing runtime directory ' + str(source))
        shutil.copytree(source, Path(stage) / name)


def start(slot, lock_root, runner=run):
    """Build, sign, verify and run; all failures stop before the next stage."""
    env = os.environ.copy()
    with console_lock(slot['console'], lock_root) as token:
        env.update(LAB_CONSOLE=slot['console'], LAB_PROFILE_NAME=slot['profile'],
                   LABLOCK_TOKEN=token, LAB_DIR=str(lock_root))
        with build_lock(lock_root):
            revision = prepare_source(slot, runner)
            profile = validate_profile(slot)
            env['LAB_PROFILE'] = str(profile)
            check_build_cache(slot)
            source = Path(slot['worktree']) / 'SerialPrograms'
            build = Path(slot['build_dir'])
            runner(['cmake', '-S', str(source), '-B', str(build),
                    *slot.get('cmake_args', []), '-DPA_LAB_PROFILE=' + slot['profile']], env=env)
            runner(['cmake', '--build', str(build), '--parallel', '4'], env=env)
            if git(slot, 'status', '--porcelain', '--untracked-files=all', runner=runner):
                raise ValueError('Source changed during build; refusing deployment.')
            if git(slot, 'rev-parse', 'HEAD', runner=runner) != revision:
                raise ValueError('Source revision changed during build; refusing deployment.')
            app = build / 'SerialPrograms.app'
            receipt = json.loads((app / 'Contents/MacOS/SerialPrograms.lab-build.json').read_text())
            if receipt.get('source_revision') != revision:
                raise ValueError('Linked receipt is stale for this source revision; relink through the normal build before retrying.')
            tool = Path(slot['worktree']) / 'devtools/lab/lab_instance.py'
            stage = Path(slot['stage_root']) / (revision[:12] + '-' + secrets.token_hex(4))
            stage.parent.mkdir(parents=True, exist_ok=True)
            runner([sys.executable, str(tool), 'stage', '--app', str(app), '--out', str(stage),
                    '--profile', slot['profile'], '--console', slot['console'],
                    '--bundle-id', slot['bundle_id'], '--signing-identity', slot['signing_identity']], env=env)
            copy_runtime_data(build, stage)
            runner([sys.executable, str(tool), 'verify', '--manifest', str(stage / 'lab-deployment.json'),
                    '--profile', slot['profile']], env=env)
        executable = stage / 'SerialPrograms.app/Contents/MacOS/SerialPrograms'
        print(f'Running console {slot["console"]}, profile {slot["profile"]}, source {revision}. '
              'Quit the GUI to release the console lock.', flush=True)
        # Direct child, never `open`: the console lock outlives the actual GUI process.
        runner([str(executable), '--profile', slot['profile']], cwd=str(stage), env=env)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('console', choices=['A', 'B'])
    parser.add_argument('--config', type=Path, default=DEFAULT_CONFIG)
    parser.add_argument('--plan', action='store_true', help='Print resolved setup without locks, fetch/build or launch')
    args = parser.parse_args()
    try:
        slot = load_config(args.config, args.console)
        if args.plan:
            print(json.dumps(slot, indent=2))
            return 0
        if sys.platform != 'darwin':
            raise ValueError('GUI development launching is supported only on macOS.')
        start(slot, Path(os.environ.get('LAB_DIR', str(Path.home() / '.lab'))))
    except KeyboardInterrupt:
        print('Development instance stopped; its child exited before locks were released.', file=sys.stderr)
        return 130
    except (OSError, ValueError, KeyError, TypeError, subprocess.CalledProcessError) as error:
        print('Dev instance refused: ' + str(error), file=sys.stderr)
        return 2
    return 0


if __name__ == '__main__':
    sys.exit(main())
