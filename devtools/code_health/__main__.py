"""Lint changed C++: python -m devtools.code_health [--base-ref REV] [--github]."""
import argparse
import os
import subprocess
import sys
from pathlib import Path

from . import layering, style
from .classes import measure_classes
from .findings import annotations
from .git_files import changed_files, default_base, read_revision
from .metrics import measure
from .policy import advisories, evaluate


def analyze(path, source):
    return measure(path, source) + measure_classes(path, source)


def run(root, base_ref=None, github=False):
    if base_ref is None:
        base_ref, how = default_base(root)
        print(f'Base: {base_ref} ({how}).')
    changes = changed_files(root, base_ref)
    failures, notes = [], []
    advisory = 0
    for current, original in changes:
        source = (root / current).read_text(encoding='utf-8-sig')
        previous_source = read_revision(root, base_ref, original) if original else None
        metrics = analyze(current, source)
        previous = analyze(current, previous_source) if original else []
        failures.extend(layering.check(current, source, previous_source))
        failures.extend(evaluate(metrics, previous))
        notes.extend(advisories(metrics, previous))
        style_errors, style_notes = style.check(current, source, previous_source)
        failures.extend(style_errors)
        notes.extend(style_notes)
        advisory += sum(m.check == 'file_lines' and 500 < m.value <= 1000 for m in metrics)
    print(f'Scanned {len(changes)} changed C++ files against {base_ref}.')
    print(f'Advisory: {advisory} changed files exceed 500 lines; prefer functions <=25 lines.')
    for note in notes:
        print(f'Advisory: {note}')
    for failure in failures:
        print(failure, file=sys.stderr)
    if github:
        for line in annotations('error', failures) + annotations('warning', notes):
            print(line)
    print(f'Code health: {len(failures)} failures.')
    return bool(failures)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root', type=Path, default=Path.cwd())
    parser.add_argument('--base-ref', help='Compare changed files with this revision (default: '
                        'merge-base of HEAD and upstream/main, else origin/main)')
    parser.add_argument('--github', action='store_true',
                        help='Also print findings as GitHub Actions annotations '
                        '(default when GITHUB_ACTIONS=true)')
    args = parser.parse_args()
    github = args.github or os.environ.get('GITHUB_ACTIONS') == 'true'
    try:
        return run(args.root.resolve(), args.base_ref, github)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f'Code-health setup/analysis error: {error}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
