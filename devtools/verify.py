#!/usr/bin/env python3
"""Run harness checks, build a configured CMake tree, then native C++ tests.

Run from a code checkout: python <agents checkout>/devtools/verify.py --build-dir ... --binary ...
The linter comes from the agents checkout that holds this file; the C++ comes from the cwd."""
import argparse
import os
import subprocess
import sys
from pathlib import Path


TOOLS = Path(__file__).resolve().parents[1]


def commands(build_dir, binary, base_ref, jobs, platform=sys.platform, root=None):
    root = Path.cwd() if root is None else root
    profile = ['--profile', 'agent-native-tests'] if platform == 'darwin' else []
    return [
        [sys.executable, '-m', 'unittest', 'discover', '-s', str(TOOLS / 'devtools/code_health/tests'),
         '-t', str(TOOLS), '-v'],
        [sys.executable, '-m', 'devtools.code_health', '--root', str(root), '--base-ref', base_ref],
        ['cmake', '--build', str(build_dir), '--config', 'RelWithDebInfo', '--parallel', str(jobs)],
        [str(binary), *profile, '--command-line-test-mode'],
    ]


def verify(steps, run=subprocess.run, env=None, native_cwd=None, root=None):
    root = Path.cwd() if root is None else root
    for index, command in enumerate(steps):
        print('Running:', ' '.join(command), flush=True)
        # The linter steps import devtools from TOOLS; cmake and the binary need the code checkout.
        directory = TOOLS if index < 2 else root
        if native_cwd and index == len(steps) - 1:
            directory = native_cwd
        result = run(command, cwd=directory, env=env)
        if result.returncode:
            return result.returncode
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--binary', type=Path, required=True)
    parser.add_argument('--base-ref', default='origin/main')
    parser.add_argument('--jobs', type=int, default=4)
    args = parser.parse_args()
    build_dir = args.build_dir.resolve()
    if not (build_dir / 'CMakeCache.txt').is_file():
        parser.error('Configure this checkout with CMake first; see devtools/code_health/README.md on agents')
    env = os.environ.copy()
    env['QT_QPA_PLATFORM'] = 'offscreen'
    env['CODE_HEALTH_REPO'] = str(Path.cwd())
    try:
        return verify(commands(build_dir, args.binary.resolve(), args.base_ref, args.jobs),
                      env=env, native_cwd=build_dir)
    except OSError as error:
        print(f'Verification could not run: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
