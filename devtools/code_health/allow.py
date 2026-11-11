"""Justified per-site bypasses: `// code-health: allow <check>[,<check>] -- <justification>`.

A tag on a function's or class's first line, or in the comment block directly above it, lets
that function's or class's listed metrics exceed their limit up to a hard ceiling. A
`file_lines` or `header_symbols` tag goes in the first FILE_TAG_LINES lines. A STYLE_CHECKS tag
(`char_pointer`, `overload_prefix`, `out_param_order`, `string_concat`) waives that style finding
on its own line or the line below its comment block. Every bypass is printed, quoting its
justification, so reviewers see it; malformed, unknown or misplaced tags fail."""
import re
from dataclasses import dataclass

from .config import CLASS_LIMITS, HEADER_LIMITS
from .findings import Finding

# Style findings a tag can bypass; they attach like a function's tag, to the finding's line.
STYLE_CHECKS = ('char_pointer', 'overload_prefix', 'out_param_order', 'string_concat')
# File-level metrics: the tag goes in the first FILE_TAG_LINES lines.
FILE_CHECKS = ('file_lines',) + tuple(HEADER_LIMITS)
CHECKS = (('file_lines', 'function_lines', 'nesting', 'parameters') + tuple(CLASS_LIMITS)
          + tuple(HEADER_LIMITS) + STYLE_CHECKS)
FILE_TAG_LINES = 20
MIN_JUSTIFICATION_CHARS = 40
MIN_JUSTIFICATION_WORDS = 6
# A bypassed value may reach at most this many times its limit. Limits of 0 are yes/no
# checks (public data, single-method interface), so a tag allows them outright.
CEILING_FACTOR = 2

MARKER = re.compile(r'//\s*code-health\s*:(.*)$')
TAG = re.compile(r'\s*allow\s+(\w+(?:\s*,\s*\w+)*)\s+--\s*(.*?)\s*$')
USAGE = 'expected "// code-health: allow <check>[,<check>] -- <justification>"'


@dataclass(frozen=True)
class Allowance:
    line: int
    checks: tuple
    justification: str


def ceiling(metric):
    """The largest value a tag can allow, or None when the limit is a yes/no check."""
    return metric.limit * CEILING_FACTOR if metric.limit else None


def parse(path, source):
    """Return (allowances, findings for malformed tags) found in `source`."""
    allowances, errors = [], []
    for number, text in enumerate(normalized_lines(source), 1):
        marker = MARKER.search(text)
        if not marker:
            continue
        tag = TAG.fullmatch(marker.group(1))
        if not tag:
            errors.append(Finding(path, number, f'malformed code-health tag; {USAGE}'))
            continue
        checks = tuple(check.strip() for check in tag.group(1).split(','))
        unknown = [check for check in checks if check not in CHECKS]
        why = tag.group(2)
        if unknown:
            errors.append(Finding(path, number, f'code-health tag: unknown check {", ".join(unknown)}; '
                                  f'known: {", ".join(CHECKS)}'))
        elif len(why) < MIN_JUSTIFICATION_CHARS or len(why.split()) < MIN_JUSTIFICATION_WORDS:
            errors.append(Finding(path, number, 'code-health tag needs a justification of at least '
                                  f'{MIN_JUSTIFICATION_CHARS} characters and {MIN_JUSTIFICATION_WORDS} '
                                  'words: why splitting would make this code worse'))
        else:
            allowances.append(Allowance(number, checks, why))
    return allowances, errors


def attach(path, source, allowances, metrics):
    """Return ({metric: allowance}, findings for tag checks that attach to nothing)."""
    lines = normalized_lines(source)
    allowed, errors = {}, []
    for allowance in allowances:
        for check in allowance.checks:
            if check in STYLE_CHECKS:
                continue
            targets = [m for m in metrics if m.check == check and covers(allowance, m, lines)]
            if not targets:
                where = (f'in the first {FILE_TAG_LINES} lines' if check in FILE_CHECKS
                         else 'on or directly above the function or class signature')
                errors.append(Finding(path, allowance.line, f'code-health tag: {check} attaches to '
                                      f'nothing; put the tag {where}'))
            for metric in targets:
                allowed[metric] = allowance
    return allowed, errors


def waive(path, source, allowances, findings):
    """(findings left, bypass notices, errors for style tags that attach to nothing).

    A STYLE_CHECKS tag goes on the finding's line or in the comment block directly above it."""
    lines = normalized_lines(source)
    kept, notices, used = [], [], set()
    for finding in findings:
        tag = next((a for a in allowances if finding.check in a.checks
                    and covers(a, finding, lines)), None)
        if tag is None:
            kept.append(finding)
            continue
        used.add((tag.line, finding.check))
        notices.append(Finding(path, finding.line, f'{finding.check} bypassed by the tag at line '
                               f'{tag.line}: "{tag.justification}"'))
    errors = [Finding(path, a.line, f'code-health tag: {check} attaches to nothing; put the tag '
                      'on or directly above the flagged line')
              for a in allowances for check in a.checks
              if check in STYLE_CHECKS and (a.line, check) not in used]
    return kept, notices, errors


def covers(allowance, metric, lines):
    if metric.check in FILE_CHECKS:
        return allowance.line <= FILE_TAG_LINES
    if allowance.line == metric.line:
        return True
    if allowance.line > metric.line or not lines[allowance.line - 1].lstrip().startswith('//'):
        return False
    between = lines[allowance.line:metric.line - 1]
    return all(text.lstrip().startswith(('//', 'template')) for text in between)


def normalized_lines(source):
    return source.replace('\r\n', '\n').replace('\r', '\n').splitlines()
