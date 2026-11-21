#!/usr/bin/env python3
"""Preview whitespace formatting of edited C/C++ lines, from any working directory.

  python3 /path/to/tools/devtools/format/format_changed.py --repo /path/to/code
  python3 devtools/format/format_changed.py --write
  python3 devtools/format/format_changed.py --base origin/main --check

Uses the config beside this tool, Uncrustify 0.83.0 and Clang's raw lexer.
Default scope is staged + unstaged tracked edits against HEAD; --base selects
its merge base with HEAD. Never stages, commits, or reformats untouched lines.
See INCREMENTAL.md for the intentionally narrow formatting scope.
"""

import argparse
from collections import Counter
from dataclasses import dataclass
import difflib
import os
from pathlib import Path
import re
import subprocess
import sys


CONFIG = Path(__file__).resolve().parents[2] / '.uncrustify.cfg'
SUFFIXES = {'.h', '.c', '.cpp', '.tpp'}


@dataclass
class Selection:
    """Changed lines tied to the exact bytes read when their Git diff was measured."""
    lines: set
    source: bytes


@dataclass
class Plan:
    """A verified candidate plus the original bytes needed to detect stale plans."""
    path: Path
    before: bytes
    after: bytes
    skipped: dict


def git(repo, *args):
    """Read Git state; never invoke shell expansion, external diff or textconv."""
    return subprocess.check_output(['git', '-C', str(repo), *args])


def hunk_lines(diff):
    """Return one-based added/modified working-tree line numbers, not deletion anchors."""
    lines = set()
    for match in re.finditer(rb'^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@', diff, re.M):
        start = int(match[1])
        count = int(match[2]) if match[2] is not None else 1
        lines.update(range(start, start + count))
    return lines


def selected_lines(repo, base):
    """Select tracked additions/modifications; ignore renamed and untracked files."""
    command = ['diff', '--no-ext-diff', '--no-textconv', '--find-renames', base]
    names = git(repo, *command, '--name-only', '--diff-filter=AM', '-z', '--', 'Common', 'SerialPrograms')
    selected = {}
    for raw in filter(None, names.split(b'\0')):
        name = os.fsdecode(raw)
        if Path(name).suffix not in SUFFIXES:
            continue
        path = repo / name
        if path.is_symlink():
            raise ValueError('Refusing to format a symbolic link: ' + name)
        source = path.read_bytes()
        diff = git(repo, *command, '--no-color', '--unified=0', '--', ':(literal)' + name)
        if path.read_bytes() != source:
            raise ValueError('Source changed while measuring the diff: ' + name)
        if hunk_lines(diff):
            selected[name] = Selection(hunk_lines(diff), source)
    return selected


def protected_lines(lines):
    """Protect preprocessor directives and their backslash continuation lines."""
    protected = set()
    continuation = False
    for index, line in enumerate(lines):
        if continuation or line.lstrip().startswith(b'#'):
            protected.add(index + 1)
            continuation = line.rstrip(b'\r\n').rstrip(b' \t').endswith(b'\\')
    return protected


def local_edits(source, formatted, selected):
    """Keep same-line whitespace edits in scope; retain indentation and line endings."""
    original = source.splitlines(keepends=True)
    output = formatted.splitlines(keepends=True)
    result = original.copy()
    protected = protected_lines(original)
    skipped = Counter()
    for tag, start, end, new_start, new_end in difflib.SequenceMatcher(
        None, original, output, autojunk=False
    ).get_opcodes():
        if tag == 'equal':
            continue
        if end - start != new_end - new_start:
            skipped['reflow'] += len(selected.intersection(range(start + 1, end + 1)))
            continue
        for index, replacement in zip(range(start, end), output[new_start:new_end]):
            if index + 1 not in selected:
                continue
            old = original[index]
            if index + 1 in protected or re.search(rb'\b(class|struct|union)\b', old):
                skipped['protected'] += 1
                continue
            if b''.join(old.split()) != b''.join(replacement.split()):
                raise ValueError('Formatter proposed a non-whitespace rewrite on an edited line.')
            body = old.rstrip(b'\r\n')
            prefix = body[:len(body) - len(body.lstrip(b' \t'))]
            result[index] = prefix + replacement.rstrip(b'\r\n').lstrip(b' \t') + old[len(body):]
    return b''.join(result), dict(skipped)


def raw_tokens(source, clang):
    """Get significant token kinds/spellings without includes, preprocessing or execution."""
    result = subprocess.run(
        [clang, '-x', 'c++', '-std=c++23', '-fsyntax-only', '-Xclang', '-dump-raw-tokens', '-'],
        input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    )
    tokens = []
    for record in re.split(rb'\tLoc=<<stdin>:\d+:\d+>\r?\n', result.stderr):
        if not record:
            continue
        match = re.match(rb"^(\w+) '(.*)'\t", record, re.S)
        if match is None:
            raise ValueError('Could not parse Clang raw token output; no file was written.')
        kind, spelling = match.groups()
        if kind == b'comment' or (kind == b'unknown' and spelling.isspace()):
            continue
        tokens.append((kind, spelling))
    return tokens


def verify_tokens(source, candidate, clang):
    """Reject whitespace changes that merge identifiers/operators or alter string literals."""
    if raw_tokens(source, clang) != raw_tokens(candidate, clang):
        raise ValueError('C++ token kind/spelling changed; no file was written.')


def make_plans(repo, selection, uncrustify, clang):
    """Verify every candidate before allowing the caller to write any source file."""
    version = subprocess.check_output([uncrustify, '--version'], text=True).strip()
    if re.fullmatch(r'Uncrustify-0\.83\.0(?:_f)?', version) is None:
        raise ValueError('Expected Uncrustify 0.83.0; found ' + version)
    plans = []
    for name, selected in selection.items():
        path = repo / name
        if path.is_symlink():
            raise ValueError('Refusing to format a symbolic link: ' + name)
        source = selected.source
        if path.read_bytes() != source:
            raise ValueError('Source changed after measuring the diff: ' + name)
        result = subprocess.run(
            [uncrustify, '-q', '-c', str(CONFIG), '-l', 'CPP'], input=source,
            stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        if result.returncode or result.stderr:
            raise ValueError(name + ': ' + result.stderr.decode(errors='replace'))
        candidate, skipped = local_edits(source, result.stdout, selected.lines)
        if candidate != source:
            verify_tokens(source, candidate, clang)
        plans.append(Plan(path, source, candidate, skipped))
    return plans


def write_plans(plans):
    """Keep the index untouched; refuse plans invalidated by a concurrent source edit."""
    for plan in plans:
        if plan.path.is_symlink() or plan.path.read_bytes() != plan.before:
            raise ValueError('Source changed after planning: ' + str(plan.path))
    for plan in plans:
        if plan.after != plan.before:
            if plan.path.is_symlink() or plan.path.read_bytes() != plan.before:
                raise ValueError('Source changed before writing: ' + str(plan.path))
            plan.path.write_bytes(plan.after)


def main(argv=None):
    """Preview by default; --write applies, --check returns 1 for pending local edits."""
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--repo', type=Path, default=Path.cwd())
    parser.add_argument('--base', default='HEAD', help='Compare the working tree with this revision\'s merge base with HEAD')
    parser.add_argument('--uncrustify', default=os.environ.get('UNCRUSTIFY', 'uncrustify'))
    parser.add_argument('--clang', default=os.environ.get('CLANG', 'clang'))
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument('--write', action='store_true')
    mode.add_argument('--check', action='store_true')
    args = parser.parse_args(argv)
    try:
        repo = Path(os.fsdecode(git(args.repo, 'rev-parse', '--show-toplevel')).strip())
        base = os.fsdecode(git(repo, 'merge-base', args.base, 'HEAD')).strip()
        selection = selected_lines(repo, base)
        plans = make_plans(repo, selection, args.uncrustify, args.clang) if selection else []
        for plan in plans:
            name = plan.path.relative_to(repo).as_posix()
            sys.stdout.writelines(difflib.unified_diff(
                plan.before.decode(errors='backslashreplace').splitlines(keepends=True),
                plan.after.decode(errors='backslashreplace').splitlines(keepends=True),
                fromfile='a/' + name, tofile='b/' + name,
            ))
            if plan.skipped:
                print(name + ': skipped ' + str(plan.skipped), file=sys.stderr)
        if args.write:
            write_plans(plans)
        count = sum(plan.before != plan.after for plan in plans)
        print(f'{count} file(s) with eligible edited-line formatting; index untouched.', file=sys.stderr)
        return int(args.check and count > 0)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('Formatting refused: ' + str(error), file=sys.stderr)
        return 2


if __name__ == '__main__':
    sys.exit(main())
