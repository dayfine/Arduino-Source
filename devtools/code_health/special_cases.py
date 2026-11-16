"""No per-checkpoint or per-event special cases in AutoStory code: `special_case`.

`if (checkpoint.id == "17.03")` or `if (event.name == "right_of_takashi")` patches one story
point instead of fixing the rule that failed there; the next checkpoint with the same shape
fails the same way (the user, 2026-11-15: "make sure our solutions are systematic and avoid
patching here and there"). What differs per checkpoint or event belongs in its plan
(`StoryData/*.plan.json`); what is common belongs in a general rule.

Checked: non-test C++ under an `AutoStory/` directory of `SerialPrograms/Source`, on the lines
a change adds. Reported: a string literal that is a checkpoint id (`NN.NN`, `NN.NNb`) or the id
of an event some plan declares, used as a key: compared with `==` / `!=` / `<=>`, an argument
of `find`, `count`, `contains`, `at`, `starts_with`, `ends_with`, `compare` or `[...]`, or the
first element of a brace pair `{"id", ...}` (a map entry). Not reported: plain lists of ids
(a checkpoint table), ids passed to other functions, and literals that are also an event's
`kind` or `action` (those name a general handler, not one event), and one-word event ids such as "bicycle",
which are also item or species names (`item == "bicycle"` in FRLG's item use)."""
import json
import re
import subprocess
from pathlib import PurePosixPath

from .cpp_text import normalize, string_literals
from .exposure import test_file
from .findings import Finding

CHECK = 'special_case'
CITE = 'user, 2026-11-15: systematic fixes, no patching here and there'
CHECKPOINT_ID = re.compile(r'\d\d\.\d\d[a-z]?')
COMPARE_BEFORE = re.compile(r'(?:==|!=|<=>)\s*$')
COMPARE_AFTER = re.compile(r'^\s*(?:==|!=|<=>)')
KEY_CALL = re.compile(r'(?:\.|->)\s*(?:find|count|contains|at|starts_with|ends_with|compare)\s*\(\s*$')
SUBSCRIPT = re.compile(r'\[\s*$')
MAP_ENTRY_BEFORE = re.compile(r'(?:^|[{,(=]\s*)\{\s*$')
MAP_ENTRY_AFTER = re.compile(r'^\s*,')
PLANS = 'StoryData/'


def autostory_code(path):
    parts = PurePosixPath(path).parts
    return (path.startswith('SerialPrograms/Source/') and 'AutoStory' in parts[:-1]
            and PLANS.rstrip('/') not in parts and not test_file(path))


def plan_names(texts):
    """(event ids, kind and action names) declared by the plan JSON `texts`."""
    events, handlers = set(), set()
    for text in texts:
        try:
            plan = json.loads(text)
        except ValueError:
            continue
        for event in plan_events(plan):
            if isinstance(event.get('id'), str):
                events.add(event['id'])
            for field in ('kind', 'action'):
                if isinstance(event.get(field), str):
                    handlers.add(event[field])
    # A one-word id ("bicycle", "heal") is also an item, species or menu name the code may
    # compare for its own reasons; an id with an underscore names one story point.
    return {e for e in events - handlers if '_' in e}, handlers


def plan_events(plan):
    checkpoint = plan.get('checkpoint') if isinstance(plan, dict) else None
    events = checkpoint.get('events') if isinstance(checkpoint, dict) else None
    return [e for e in events if isinstance(e, dict)] if isinstance(events, list) else []


def tracked_plans(root):
    """Texts of every `StoryData/*.plan.json` in the working tree."""
    result = subprocess.run(['git', '-C', str(root), 'ls-files', '--', '*StoryData/*.plan.json'],
                            capture_output=True, text=True)
    if result.returncode != 0:
        raise ValueError(f'git ls-files for plans failed: {result.stderr.strip()}')
    return [(root / path).read_text(encoding='utf-8', errors='replace')
            for path in result.stdout.splitlines() if (root / path).is_file()]


def key_use(line, start, end):
    """Whether the literal at [start, end) of `line` is compared or looked up."""
    before, after = line[:start], line[end:]
    return bool(COMPARE_BEFORE.search(before) or COMPARE_AFTER.match(after)
                or KEY_CALL.search(before) or (SUBSCRIPT.search(before) and after.lstrip().startswith(']'))
                or (MAP_ENTRY_BEFORE.search(before) and MAP_ENTRY_AFTER.match(after)))


def check(path, source, added, event_ids):
    """Findings for checkpoint or event ids used as keys on `added` lines of AutoStory code."""
    if not autostory_code(path):
        return []
    lines = normalize(source).split('\n')
    findings = []
    for line, start, end, value in string_literals(source):
        if line not in added or not (CHECKPOINT_ID.fullmatch(value) or value in event_ids):
            continue
        if key_use(lines[line - 1], start, end):
            findings.append(Finding(path, line, f'special case by name: "{value}" at {path}:{line}; '
                                    f'move it into the plan or a general rule ({CITE})', check=CHECK))
    return findings
