"""Lint changed C++: python -m devtools.code_health --base-ref origin/main."""
import argparse
import subprocess
import sys
from pathlib import Path

from .git_files import changed_files, read_revision
from .metrics import measure
from .policy import evaluate


def run(root, base_ref):
    changes = changed_files(root, base_ref)
    failures = []
    advisory = 0
    for current, original in changes:
        metrics = measure(current, (root / current).read_text(encoding='utf-8-sig'))
        previous = measure(current, read_revision(root, base_ref, original)) if original else []
        failures.extend(evaluate(metrics, previous))
        advisory += sum(m.check == 'file_lines' and 500 < m.value <= 1000 for m in metrics)
    print(f'Scanned {len(changes)} changed C++ files against {base_ref}.')
    print(f'Advisory: {advisory} changed files exceed 500 lines; prefer functions <=25 lines.')
    for failure in failures:
        print(failure, file=sys.stderr)
    print(f'Code health: {len(failures)} failures.')
    return bool(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--base-ref', default='origin/main', help='Compare changed files with this revision')
    args = parser.parse_args()
    try:
        return run(args.root.resolve(), args.base_ref)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Code-health setup/analysis error: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
