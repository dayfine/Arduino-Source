"""A rule learned on the console needs a test: `learned_rule_test`.

A comment in non-test C++ that cites a console finding, "(console B, 2026-11-10)" or
"(17.03, console B 2026-11-15)", records a rule the code learned from a run. That rule is kept
only if a test fails when it breaks (the user, 2026-11-15: "make sure our solutions are
systematic and avoid patching here and there"). So each citation needs a test that cites the
same finding: a `*_Tests.cpp` / `*_Tests.h` file, or a `ConsoleFailures/*.json` replay, that
names the same date and, when the citation names a checkpoint id, that checkpoint too. The test
lives in the same game's directory (`VideoGames/<Game>/`, `PokemonBDSP/`), or in shared code
outside those, which tests every game.

Reported, on lines this change adds: a citation no test covers, unless the base file cited the
same date (and checkpoint) already, so rewording an old comment is not a new rule. And for the whole tree: a
citation that a test covered at the base and none covers now, i.e. the change deleted or edited
the last test that cited it. A citation is "console B" or "lab-B", then within the same clause
(no `(`, `)`, `;` or sentence end between) a date; the checkpoint is a `NN.NN[a-z]` id in that
clause or in the parenthesis that holds the citation."""
import re
import subprocess
from dataclasses import dataclass
from pathlib import PurePosixPath

from .cpp_text import comment_blocks, normalize
from .exposure import test_file
from .findings import Finding
from .git_files import EXTENSIONS, git, included

CHECK = 'learned_rule_test'
CITE = 'user, 2026-11-15: rules learned on the console are kept by tests'
CONSOLE = re.compile(r'\b(?:console|lab)[ -]B\b', re.I)
DATE = re.compile(r'\b20\d\d-[01]\d-[0-3]\d\b')
CHECKPOINT = re.compile(r'(?<![\w.])\d\d\.\d\d[a-z]?(?![\w.]*\d)')
CLAUSE_END = re.compile(r'[();]|\.\s|\.$')
# Characters after "console B" in which the date must start: "console B, FRLG 07.02 run 2, 2026-11-09".
WINDOW = 48
REPLAYS = 'ConsoleFailures/'


@dataclass(frozen=True)
class Citation:
    path: str
    line: int
    date: str
    checkpoint: str  # '' when the citation names none

    def label(self):
        where = f'{self.checkpoint}, ' if self.checkpoint else ''
        return f'({where}console B, {self.date})'


def is_test(path):
    return test_file(path) or (REPLAYS in path and path.endswith('.json'))


def area(path):
    """The game directory a file belongs to, or '' for shared code."""
    parts = PurePosixPath(path).parts
    for index, part in enumerate(parts):
        if part == 'VideoGames' and index + 1 < len(parts):
            return '/'.join(parts[:index + 2])
        if part == 'PokemonBDSP':
            return '/'.join(parts[:index + 1])
    return ''


def citations(path, source):
    """Citations in the comments of `source`, at the line where "console B" stands."""
    found = []
    for block in comment_blocks(source):
        lines = sorted(block)
        text, starts = '', []
        for number in lines:
            starts.append((len(text), number))
            text += block[number] + ' '
        for console in CONSOLE.finditer(text):
            after = text[console.end():console.end() + WINDOW]
            end = CLAUSE_END.search(after)
            clause = after[:end.start()] if end else after
            date = DATE.search(clause)
            if not date:
                continue
            opening = text.rfind('(', 0, console.start())
            closing = text.rfind(')', 0, console.start())
            before = text[opening + 1:console.start()] if opening > closing else ''
            checkpoint = CHECKPOINT.search(before + ' ' + clause[:date.start()])
            line = max(number for offset, number in starts if offset <= console.start())
            found.append(Citation(path, line, date.group(), checkpoint.group() if checkpoint else ''))
    return found


@dataclass(frozen=True)
class TestCite:
    path: str
    dates: frozenset
    checkpoints: frozenset


def test_cite(path, text):
    return TestCite(path, frozenset(DATE.findall(text)), frozenset(CHECKPOINT.findall(text)))


def covers(test, citation):
    if test.path == citation.path or citation.date not in test.dates:
        return False
    if citation.checkpoint and citation.checkpoint not in test.checkpoints:
        return False
    home = area(citation.path)
    return not home or area(test.path) in ('', home)


def covered(citation, tests):
    return any(covers(test, citation) for test in tests)


def tracked_tests(root, revision=None):
    """TestCites for every test file at `revision` (None: the working tree) that names a date."""
    args = ['grep', '-l', '-I', '-E', '20[0-9]{2}-[01][0-9]-[0-3][0-9]',
            *([revision] if revision else []), '--', 'Common', 'SerialPrograms']
    result = subprocess.run(['git', '-C', str(root), *args], capture_output=True, text=True)
    if result.returncode not in (0, 1):
        raise ValueError(f'git grep for test citations failed: {result.stderr.strip()}')
    tests = []
    for line in result.stdout.splitlines():
        path = line.split(':', 1)[1] if revision else line
        if not is_test(path):
            continue
        if revision:
            text = git(root, 'show', f'{revision}:{path}').decode('utf-8', 'replace')
        else:
            text = (root / path).read_text(encoding='utf-8', errors='replace')
        tests.append(test_cite(path, text))
    return tests


def code_citations(root, revision=None):
    """Citations in every non-test first-party C++ file at `revision` (None: the working tree)."""
    args = ['grep', '-l', '-I', '-i', '-E', r'(console|lab)[ -]B',
            *([revision] if revision else []), '--', 'Common', 'SerialPrograms']
    result = subprocess.run(['git', '-C', str(root), *args], capture_output=True, text=True)
    if result.returncode not in (0, 1):
        raise ValueError(f'git grep for console citations failed: {result.stderr.strip()}')
    found = []
    for line in result.stdout.splitlines():
        path = line.split(':', 1)[1] if revision else line
        if not included(path) or is_test(path) or PurePosixPath(path).suffix not in EXTENSIONS:
            continue
        if revision:
            source = git(root, 'show', f'{revision}:{path}').decode('utf-8', 'replace')
        else:
            source = (root / path).read_text(encoding='utf-8', errors='replace')
        found.extend(citations(path, source))
    return found


def message(citation, why='learned rule without a test'):
    return (f'{why}: {citation.path}:{citation.line} {citation.label()}; add a test or a '
            f'ConsoleFailures replay that cites it ({CITE})')


def already_cited(citation, before):
    """Whether the base file cited this finding already: a reworded comment is no new rule."""
    return any(old.date == citation.date and citation.checkpoint in ('', old.checkpoint) for old in before)


def check(path, source, previous_source, added, tests):
    """Findings for new citations on `added` lines of a non-test C++ file that no test covers.

    A citation of a date (and checkpoint) the base file already cited is not new."""
    if is_test(path):
        return []
    before = citations(path, normalize(previous_source)) if previous_source is not None else []
    return [Finding(path, c.line, message(c), check=CHECK)
            for c in citations(path, normalize(source))
            if c.line in added and not already_cited(c, before) and not covered(c, tests)]


def lost_coverage(current_code, base_tests, tests):
    """Findings for citations a base test covered that no current test covers."""
    return [Finding(c.path, c.line, message(c, 'learned rule lost its last test'), check=CHECK)
            for c in current_code if covered(c, base_tests) and not covered(c, tests)]
