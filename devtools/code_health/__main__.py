"""Usage: python -m devtools.code_health [--base-ref origin/main]."""
import argparse
import datetime
import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

from .git_files import git, read_revision, tracked
from .metrics import measure
from .policy import evaluate, make_baseline, read_entries


BASELINE = 'devtools/code_health/baseline.json'


def scan(root, revision=None):
    metrics = []
    paths = tracked(root, revision)
    for path in paths:
        source = (read_revision(root, revision, path) if revision else
                  (root / path).read_text(encoding='utf-8-sig'))
        metrics.extend(measure(path, source))
    return paths, metrics


def previous_metrics(root, base_ref, metrics):
    changed = set(git(root, 'diff', '--name-only', '-z', base_ref, '--').decode().split('\0'))
    existing = set(tracked(root, base_ref))
    old = [metric for metric in metrics if metric.path not in changed]
    for path in sorted(changed & existing):
        old.extend(measure(path, read_revision(root, base_ref, path)))
    return old


def trusted_baseline(root, base_ref):
    if base_ref:
        files = git(root, 'ls-tree', '--name-only', base_ref, BASELINE).decode().splitlines()
        if files:
            return json.loads(read_revision(root, base_ref, BASELINE))
        print('Bootstrap: base has no baseline; validating the initial baseline in this tree.')
    return json.loads((root / BASELINE).read_text())


def run(args):
    root = args.root.resolve()
    paths, metrics = scan(root)
    violations = [m for m in metrics if m.value > m.limit]
    print(f'Scanned {len(paths)} tracked C++ files; '
          f'{len(violations)} violations: {dict(Counter(m.check for m in violations))}')
    soft = sum(m.check == 'file_lines' and 500 < m.value <= 1000 for m in metrics)
    print(f'Advisory: {soft} files exceed 500 lines; prefer functions <=25 lines.')
    if args.write_baseline:
        baseline = make_baseline(metrics, args.review_at)
        (root / BASELINE).write_text(json.dumps(baseline, indent=2) + '\n')
        print(f'Wrote {len(baseline["violations"])} dated baseline entries. Review before committing.')
        return 0
    baseline = trusted_baseline(root, args.base_ref)
    # Validate the working copy too; CI must not accept malformed edits to the registry.
    read_entries(json.loads((root / BASELINE).read_text()))
    previous = previous_metrics(root, args.base_ref, metrics) if args.base_ref else None
    failures = evaluate(metrics, baseline, datetime.date.today(), previous)
    for failure in failures:
        print(failure, file=sys.stderr)
    print(f'Code health: {len(failures)} failures.')
    return bool(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--base-ref', help='Trusted PR base; ratchet against its actual measurements')
    parser.add_argument('--write-baseline', action='store_true', help='Initial rollout only; not a fix')
    parser.add_argument('--review-at', default=(datetime.date.today() + datetime.timedelta(days=90)).isoformat())
    args = parser.parse_args()
    try:
        return run(args)
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        print(f'Code-health setup/analysis error: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
