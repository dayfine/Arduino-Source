"""Promote PLZA plan rows from "save": "unverified" to "autosave" once a run log shows their autosave.

A row's autosave counts as seen when a run of that checkpoint passed and logged the autosave badge
in the row's last step ("[PLZA] autosave badge at <s>s in <step>", older logs "[Autosave] <id>/<n> <step>"),
or logged "[Save] <id>: the game autosaved (seen)" with a badge, not a menu save, as the run's last sighting.
A badge in an earlier step means the checkpoint should be split there, and a badge in a failed run
proves nothing about where the row ends: both are reported, never promoted.
"""

import json
import re
from dataclasses import dataclass
from pathlib import Path

PROGRAM = re.compile(r"Starting Program: ([^:<\s]+):")
LINE = re.compile(r"^\ufeff?(\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)\.\d+ - \[[^\]]*\]: (.*)$")
START = re.compile(r"^\[Checkpoint\] (\S+): start\b")
PASSED = re.compile(r"^\[Checkpoint\] (\S+): passed\b")
FAILED = re.compile(r"^\[Checkpoint\] (\S+): failed\b")
BADGE = re.compile(r"^\[PLZA\] autosave badge at [\d.]+s in (\S+)$")
OLD_BADGE = re.compile(r"^\[Autosave\] (\S+)/\d+ (\S+)$")
MENU_SAVE = re.compile(r"^\[PLZA\] saved from the menu in (\S+)$")
SEEN = re.compile(r"^\[Save\] (\S+): the game autosaved \(seen\)$")

GAME = "PokemonLZA"

SAVE_LINE = re.compile(r'^([ \t]*)"save": "unverified"(,?)(\r?\n)', re.MULTILINE)
SOURCE = re.compile(r'("source": ")((?:[^"\\]|\\.)*)(")')
UNVERIFIED_SENTENCE = re.compile(r'Save unverified:(?:[^"\\]|\\.)*?\.(?=$| [A-Z])')


@dataclass(frozen=True)
class Sighting:
    row: str
    #  "end": in the last step of a passed run; "mid": in an earlier step of a passed run;
    #  "failed": in a run that did not pass; "none": a passed run with no badge (step is empty).
    kind: str
    step: str
    when: str
    log: str = ""


@dataclass
class _Run:
    row: str
    #  (kind, step, when), kind "badge" or "menu", in log order.
    looks: list
    #  when the run started
    looks_from: str
    seen: bool = False


def _close(run, passed, row_steps):
    steps = row_steps.get(run.row)
    if not steps:
        return []
    badges = [(step, when) for kind, step, when in run.looks if kind == "badge" and step in steps]
    if not badges:
        return [Sighting(run.row, "none", "", run.looks_from)] if passed else []
    if not passed:
        step, when = badges[-1]
        return [Sighting(run.row, "failed", step, when)]
    last_step = steps[-1]
    at_end = [(step, when) for step, when in badges if step == last_step]
    if at_end:
        return [Sighting(run.row, "end", last_step, at_end[-1][1])]
    if run.seen and run.looks[-1][0] == "badge":
        return [Sighting(run.row, "end", last_step, badges[-1][1])]
    step, when = badges[0]
    return [Sighting(run.row, "mid", step, when)]


def read_sightings(lines, row_steps):
    """Autosave sightings per checkpoint run in log `lines`, for rows in `row_steps` (id -> step ids)."""
    found = []
    run = None
    #  the program running, from "Starting Program: <game>:<program>"; None before the first.
    program = None
    for line in lines:
        if m := PROGRAM.search(line):
            program = m.group(1)
            if run:
                found += _close(run, False, row_steps)
            run = None
            continue
        match = LINE.match(line.rstrip("\r\n"))
        if not match:
            continue
        when, text = match.groups()
        if m := START.match(text):
            if run:
                found += _close(run, False, row_steps)
            run = _Run(m.group(1), [], when) if program in (None, GAME) else None
        elif run is None:
            continue
        elif m := PASSED.match(text):
            if m.group(1) == run.row:
                found += _close(run, True, row_steps)
                run = None
        elif m := FAILED.match(text):
            if m.group(1) == run.row:
                found += _close(run, False, row_steps)
                run = None
        elif m := BADGE.match(text):
            run.looks.append(("badge", m.group(1), when))
        elif m := OLD_BADGE.match(text):
            if m.group(1) == run.row:
                run.looks.append(("badge", m.group(2), when))
        elif m := MENU_SAVE.match(text):
            run.looks.append(("menu", m.group(1), when))
        elif m := SEEN.match(text):
            run.seen = run.seen or m.group(1) == run.row
    if run:
        found += _close(run, False, row_steps)
    return found


def promote_text(text, step, when):
    """`text`, a plan file, with "save": "unverified" made "autosave" at `step`, citing the log at `when`."""
    def save_line(m):
        indent, comma, newline = m.groups()
        return f'{indent}"save": "autosave",{newline}{indent}"autosave_step": "{step}"{comma}{newline}'

    text, count = SAVE_LINE.subn(save_line, text, count=1)
    if count != 1:
        raise ValueError('no "save": "unverified" line')
    citation = (
        f"Autosave: a run log showed the badge in {step}, its last step, "
        f"and the checkpoint passed (console B, {when})."
    )

    def source(m):
        head, body, tail = m.groups()
        body, replaced = UNVERIFIED_SENTENCE.subn(citation, body, count=1)
        if not replaced:
            body = f"{body}{'' if body.endswith('.') else '.'} {citation}" if body else citation
        return head + body + tail

    return SOURCE.sub(source, text, count=1)


def _plans(plan_dir):
    plans = {}
    for path in sorted(Path(plan_dir).glob("plza-*.plan.json")):
        data = json.loads(path.read_bytes())
        row = data["checkpoint"]["id"]
        plans[row] = (path, [event["id"] for event in data["checkpoint"]["events"]], data["execution"].get("save"))
    return plans


def run(log_paths, plan_dir, apply=False):
    """Report (and with `apply`, write) which unverified rows the logs show autosaving at their end."""
    plans = _plans(plan_dir)
    row_steps = {row: steps for row, (_, steps, _) in plans.items()}
    sightings = []
    for log_path in log_paths:
        with open(log_path, encoding="utf-8", errors="replace") as file:
            sightings += [
                Sighting(s.row, s.kind, s.step, s.when, Path(log_path).name) for s in read_sightings(file, row_steps)
            ]
    out = []
    for row, (path, _, save) in plans.items():
        if save != "unverified":
            continue
        mine = [s for s in sightings if s.row == row]
        ends = [s for s in mine if s.kind == "end"]
        misses = [s for s in mine if s.kind in ("mid", "none")]
        if ends and not misses:
            best = max(ends, key=lambda s: s.when)
            verb = "promoted" if apply else "would promote"
            out.append(f"{verb} {row}: autosave in {best.step} of a passed run at {best.when} ({best.log}), {len(ends)} run(s)")
            if apply:
                raw = path.read_bytes().decode("utf-8")
                path.write_bytes(promote_text(raw, best.step, best.when).encode("utf-8"))
            continue
        if not mine:
            out.append(f"unverified {row}: no autosave sighting")
            continue
        notes = "; ".join(f"{s.kind} {s.step} at {s.when} ({s.log})".replace("  ", " ") for s in mine)
        out.append(f"unverified {row}: {len(ends)} passed run(s) saw it at the end, {len(misses)} did not: {notes}")
    return "\n".join(out) + "\n"
