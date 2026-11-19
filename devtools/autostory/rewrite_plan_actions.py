#!/usr/bin/env python3
"""Rewrite AutoStory plan files from {"kind": K, "action": A?} steps to {"action": N}.

N is A when the step names one, else the kind's action name: the kind itself
("talk", "scene", ...) unless --default KIND=NAME says otherwise. Only the
"kind" member of each event is touched, so the rest of the file keeps its
formatting and line endings; a file with no "kind" is left as it is.

    python3 devtools/autostory/rewrite_plan_actions.py [--dry-run] [--default scene=mash] FILE...
"""

import argparse
import json
import re
import sys

KIND = re.compile(r'"kind"\s*:\s*"([^"]*)"')


def events_of(plan):
    """The event objects of a plan file's checkpoint."""
    checkpoint = plan.get("checkpoint", plan)
    return checkpoint.get("events", [])


def remove_member(text, match):
    """`text` without the member `match` and the comma that joins it."""
    start, end = match.span()
    line_start = text.rfind("\n", 0, start) + 1
    after = re.compile(r"[ \t]*,[ \t]*(\r?\n)?").match(text, end)
    if after and text[line_start:start].strip() == "" and after.group(1):
        return text[:line_start] + text[after.end():]
    if after:
        return text[:start] + text[after.end():]
    before = re.search(r",\s*$", text[:start])
    return text[:before.start()] + text[end:]


def rewrite(text, defaults):
    """`text` with every event's "kind" rewritten; raises ValueError when the
    "kind" members in the text are not exactly the events' ones."""
    events = events_of(json.loads(text))
    kinds = [event for event in events if "kind" in event]
    matches = list(KIND.finditer(text))
    if len(matches) != len(kinds):
        raise ValueError(f'{len(matches)} "kind" members in the text, {len(kinds)} in events')
    # Back to front, so earlier offsets stay valid.
    for match, event in reversed(list(zip(matches, kinds))):
        if match.group(1) != event["kind"]:
            raise ValueError(f'"kind" {match.group(1)} out of order with event {event.get("id")}')
        if "action" in event:
            text = remove_member(text, match)
        else:
            name = defaults.get(event["kind"], event["kind"])
            text = text[:match.start()] + f'"action": "{name}"' + text[match.end():]
    expected = []
    for event in events:
        kind = event.get("kind")
        event = {key: value for key, value in event.items() if key != "kind"}
        if kind is not None:
            event.setdefault("action", defaults.get(kind, kind))
        expected.append(event)
    if events_of(json.loads(text)) != expected:
        raise ValueError("the rewritten events differ from the expected ones")
    return text


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("files", nargs="+")
    parser.add_argument("--dry-run", action="store_true", help="report what would change; write nothing")
    parser.add_argument("--default", action="append", default=[], metavar="KIND=NAME",
                        help="the action a step of KIND with no action runs (default: KIND)")
    args = parser.parse_args(argv)
    defaults = dict(item.split("=", 1) for item in args.default)
    failed = False
    changed = 0
    for path in args.files:
        with open(path, encoding="utf-8", newline="") as file:
            text = file.read()
        try:
            result = rewrite(text, defaults)
        except (ValueError, json.JSONDecodeError) as error:
            print(f"{path}: {error}", file=sys.stderr)
            failed = True
            continue
        if result == text:
            continue
        changed += 1
        removed = len(KIND.findall(text))
        print(f"{path}: {removed} steps")
        if not args.dry_run:
            with open(path, "w", encoding="utf-8", newline="") as file:
                file.write(result)
    print(f"{changed} of {len(args.files)} files {'would change' if args.dry_run else 'changed'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
