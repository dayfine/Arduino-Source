#!/usr/bin/env python3
"""Run harness checks, build a configured CMake tree, then native C++ tests."""
import argparse
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def commands(build_dir, binary, base_ref, jobs):
    return [
        [sys.executable, '-m', 'unittest', 'discover', '-s', 'devtools/code_health/tests', '-v'],
        [sys.executable, '-m', 'devtools.code_health', '--base-ref', base_ref],
        ['cmake', '--build', str(build_dir), '--config', 'RelWithDebInfo', '--parallel', str(jobs)],
        [str(binary), '--profile', 'agent-native-tests', '--command-line-test-mode'],
    ]


def verify(steps, run=subprocess.run, env=None):
    for command in steps:
        print('Running:', ' '.join(command), flush=True)
        result = run(command, cwd=ROOT, env=env)
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
        parser.error('Configure this checkout with CMake first; see devtools/code_health/README.md')
    env = os.environ.copy()
    env['QT_QPA_PLATFORM'] = 'offscreen'
    try:
        return verify(commands(build_dir, args.binary.resolve(), args.base_ref, args.jobs), env=env)
    except OSError as error:
        print(f'Verification could not run: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
