"""Walks are planned: a hand-written `legs` event says why the planner can't walk it.

A walk in a story plan is a planned route to a target the planner walks; a fixed list of legs
("east 1, south 6, north 1") is a recording of one run that breaks when anything differs
(the user, 2026-11-15, 17.03's hand-written legs: "not ok"). FRLG's step lint
(`test_legs_need_a_reason` in `PokemonFRLG_AutoStoryStepLint_Tests.cpp`) enforces this for
FRLG at test time; this check applies it to every game's `StoryData/*.plan.json` at review.

An event is a legs event when its `kind` or `action` is "legs" or its `args` hold a "legs"
list (FRLG `kind: legs`, PLZA `action: walk` with `args.legs`). A new legs event, or one this
change edits, must say "not planned: <reason>" in its `why`. Events already at the base and
left unchanged are grandfathered; their count per game is printed with each run."""
import json
import re
from pathlib import PurePosixPath

from .findings import Finding

NOT_PLANNED = 'not planned: '
CITE = 'user, 2026-11-15: walks are planned routes'
PLAN = re.compile(r'(?:^|/)StoryData/[^/]+\.plan\.json$')


def is_plan(path):
    return PLAN.search(path) is not None


def events(text):
    """{event id: event} of a plan's checkpoint, or None when the text is not a plan."""
    try:
        plan = json.loads(text)
    except ValueError:
        return None
    checkpoint = plan.get('checkpoint') if isinstance(plan, dict) else None
    listed = checkpoint.get('events') if isinstance(checkpoint, dict) else None
    if not isinstance(listed, list):
        return None
    return {e['id']: e for e in listed if isinstance(e, dict) and isinstance(e.get('id'), str)}


def is_legs(event):
    args = event.get('args')
    return ('legs' in (event.get('kind'), event.get('action'))
            or (isinstance(args, dict) and isinstance(args.get('legs'), list)))


def reasoned(event):
    why = event.get('why')
    return isinstance(why, str) and NOT_PLANNED in why


def game(path, text):
    try:
        name = json.loads(text).get('game')
    except (ValueError, AttributeError):
        name = None
    return name if isinstance(name, str) else PurePosixPath(path).name.split('-')[0]


def id_line(text, event_id):
    pattern = re.compile(r'"id"\s*:\s*' + re.escape(json.dumps(event_id)))
    for number, line in enumerate(text.split('\n'), 1):
        if pattern.search(line):
            return number
    return 1


def check(path, text, previous_text):
    """Findings for new or changed legs events in plan `path` without a "not planned:" reason."""
    if not is_plan(path):
        return []
    current = events(text)
    if current is None:
        return [Finding(path, 1, 'plan lint: not a plan with checkpoint.events; cannot check its walks')]
    before = (events(previous_text) if previous_text is not None else None) or {}
    return [Finding(path, id_line(text, event_id), f'hand-written legs: {event_id} in {path} '
                    f'has no "{NOT_PLANNED}<reason>" in its why; make it a planned route to its '
                    f'target, or say why the planner cannot walk it ({CITE})')
            for event_id, event in current.items()
            if is_legs(event) and before.get(event_id) != event and not reasoned(event)]


def counts(plans):
    """{game: (legs events, legs events with a reason)} over `plans`, [(path, text)]."""
    totals = {}
    for path, text in plans:
        for event in (events(text) or {}).values():
            if is_legs(event):
                legs, with_reason = totals.get(game(path, text), (0, 0))
                totals[game(path, text)] = (legs + 1, with_reason + reasoned(event))
    return totals
