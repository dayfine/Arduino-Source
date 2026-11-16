"""Lint changed C++: python -m devtools.code_health [--base-ref REV] [--github]."""
import argparse
import os
import subprocess
import sys
from pathlib import Path

from . import allow, exposure, header_api, layering, learned_rules, param_comments, plan_lint, special_cases, style
from .classes import measure_classes
from .cpp_text import normalize
from .findings import annotations
from .git_files import changed_files, changed_paths, default_base, read_revision
from .metrics import measure
from .policy import advisories, bypasses, evaluate


def analyze(path, source):
    return measure(path, source) + measure_classes(path, source) + header_api.measure_header(path, source)


class Repository:
    """Whole-tree facts some line checks need, each read once and only when asked for."""

    def __init__(self, root):
        self.root = root
        self.users = exposure.git_users(root)
        self._tests = None
        self._event_ids = None

    @property
    def tests(self):
        if self._tests is None:
            self._tests = learned_rules.tracked_tests(self.root)
        return self._tests

    @property
    def event_ids(self):
        if self._event_ids is None:
            self._event_ids, _ = special_cases.plan_names(special_cases.tracked_plans(self.root))
        return self._event_ids


def style_findings(path, source, previous_source, repo):
    """(style errors, style advisories) on lines new since `previous_source` (None: all lines)."""
    errors, notes = style.check(path, source, previous_source)
    errors += header_api.prefix_overloads(path, source, previous_source)
    # Advisory: both report many sites a reviewer may accept (pbf_* timings, API for a
    # later PR in a stack); see the README's Style table for the measured rates.
    notes += param_comments.check(path, source, previous_source)
    notes += exposure.check(path, source, previous_source, repo.users)
    added = style.added_lines(normalize(source).split('\n'),
                              None if previous_source is None else normalize(previous_source).split('\n'))
    if special_cases.autostory_code(path):
        errors += special_cases.check(path, source, added, repo.event_ids)
    if learned_rules.CONSOLE.search(source):
        errors += learned_rules.check(path, source, previous_source, added, repo.tests)
    return errors, notes


def lost_tests(root, base_ref, paths, repo):
    """Failures for console citations whose last covering test this change removed or edited."""
    changed = {current: original for _, current, original in paths
               if learned_rules.is_test(current) or (original and learned_rules.is_test(original))}
    if not changed:
        return [], []
    base_tests = [t for t in repo.tests if t.path not in changed]
    for original in filter(None, changed.values()):
        try:
            base_tests.append(learned_rules.test_cite(original, read_revision(root, base_ref, original)))
        except subprocess.CalledProcessError:
            continue
    found = learned_rules.lost_coverage(learned_rules.code_citations(root), base_tests, repo.tests)
    kept, waived = [], []
    for path in sorted({f.path for f in found}):
        source = (root / path).read_text(encoding='utf-8-sig')
        allowances, _ = allow.parse(path, source)
        left, notices, _ = allow.waive(path, source, allowances, [f for f in found if f.path == path])
        kept += left
        waived += notices
    return kept, waived


def plan_findings(root, base_ref, paths):
    """(failures for unexplained new legs events in changed plans, per-game legs counts)."""
    failures, plans = [], []
    for status, current, original in paths:
        if status == 'D' or not plan_lint.is_plan(current):
            continue
        text = (root / current).read_text(encoding='utf-8-sig')
        previous = read_revision(root, base_ref, original) if original and plan_lint.is_plan(original) else None
        failures += plan_lint.check(current, text, previous)
        plans.append((current, text))
    return failures, plan_lint.counts(plans)


def run(root, base_ref=None, github=False):
    if base_ref is None:
        base_ref, how = default_base(root)
        print(f'Base: {base_ref} ({how}).')
    changes = changed_files(root, base_ref)
    paths = changed_paths(root, base_ref)
    repo = Repository(root)
    failures, notes, bypassed = [], [], []
    advisory = 0
    for current, original in changes:
        source = (root / current).read_text(encoding='utf-8-sig')
        previous_source = read_revision(root, base_ref, original) if original else None
        metrics = analyze(current, source)
        previous = analyze(current, previous_source) if original else []
        failures.extend(layering.check(current, source, previous_source))
        allowances, tag_errors = allow.parse(current, source)
        allowed, misplaced = allow.attach(current, source, allowances, metrics)
        failures.extend(tag_errors + misplaced)
        failures.extend(evaluate(metrics, previous, allowed))
        notes.extend(advisories(metrics, previous))
        notices, unneeded = bypasses(metrics, allowed)
        bypassed.extend(notices)
        notes.extend(unneeded)
        style_errors, style_notes = style_findings(current, source, previous_source, repo)
        # Waive errors and advisories in one pass, so a tag on either counts as used. A tag on
        # a line this change left alone attaches to the whole file's findings.
        kept, waived, unattached = allow.waive(
            current, source, allowances, style_errors + style_notes,
            whole_file=lambda: sum(style_findings(current, source, None, repo), []))
        failures.extend([f for f in kept if f in style_errors] + unattached)
        bypassed.extend(waived)
        notes.extend(f for f in kept if f not in style_errors)
        advisory += sum(m.check == 'file_lines' and 500 < m.value <= 1000 for m in metrics)
    lost, lost_waived = lost_tests(root, base_ref, paths, repo)
    failures.extend(lost)
    bypassed.extend(lost_waived)
    plan_failures, legs = plan_findings(root, base_ref, paths)
    failures.extend(plan_failures)
    print(f'Scanned {len(changes)} changed C++ files against {base_ref}.')
    for name, (total, with_reason) in sorted(legs.items()):
        print(f'Plan lint: {name}: {total} legs events in the changed plans, {with_reason} with a '
              '"not planned:" reason; only new or edited ones must have one.')
    print(f'Advisory: {advisory} changed files exceed 500 lines; prefer functions <=25 lines.')
    for note in notes:
        print(f'Advisory: {note}')
    for notice in bypassed:
        print(f'Bypass: {notice}')
    for failure in failures:
        print(failure, file=sys.stderr)
    if github:
        for line in (annotations('error', failures) + annotations('warning', notes)
                     + annotations('warning', bypassed, title='bypass')):
            print(line)
    if bypassed:
        print(f'Bypassed: {len(bypassed)} limits under code-health allow tags; review each justification.')
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
