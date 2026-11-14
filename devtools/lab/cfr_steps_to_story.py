#!/usr/bin/env python3
"""Turn a Command File Runner steps file into draft PLZA story table rows.

A story played by hand through the lab channel with the LZA commands
(PokemonLZA_AutoStoryCommands.h) leaves Lab/outbox/<batch>.steps.jsonl, one
JSON line per command (NintendoSwitch_CommandFileRecorder.h). This prints C++
for PokemonLZA_AutoStoryTable.cpp: one row per stretch between saves, one
step per action command, and the story_moves() entries the moves need.

    python3 devtools/lab/cfr_steps_to_story.py Lab/outbox/walk.steps.jsonl --first-id 01.03

Anything that is not a story action is kept as a comment marked REVIEW:
runs of raw inputs (press, dpad, stick, ...) collapse into one "RAW n inputs"
line, and failed commands, lone turns and choices past the second are named.
Reads (shot, detect, wait_for, ...) change nothing on the console and are
left out. Names are "<action>_L<line>"; titles and reasons are TODO.
"""

import argparse
import json
from pathlib import Path
import sys


#  Inputs with no story meaning of their own: a run of them is one gap.
RAW_COMMANDS = {"press", "dpad", "taps", "stick", "wait", "walk"}

#  Commands that only read the screen or the app, and "stop".
READ_COMMANDS = {"shot", "shots", "history", "detect", "wait_for", "expect", "state", "help", "stop"}

#  Commands that are one step with no arguments: kind, name.
SIMPLE_STEPS = {
    "lza_talk_to": ("TALK", "talk"),
    "lza_battle": ("BATTLE", "battle"),
    "lza_heal": ("HEAL", "heal"),
}


def read_steps(path):
    """The steps of a .steps.jsonl file, in order."""
    with Path(path).open(encoding="utf-8") as stream:
        return [json.loads(line) for line in stream if line.strip()]


def cpp_string(text):
    return json.dumps(text)


def make(kind, name, variant=None):
    variant_arg = f", {cpp_string(variant)}" if variant else ""
    return f'make(StoryStepKind::{kind}, {cpp_string(name)}, "TODO why"{variant_arg}),'


def designated(kind, name, fields):
    extra = "".join(f", .{key} = {value}" for key, value in fields)
    return f'StoryStep{{.kind = StoryStepKind::{kind}, .name = {cpp_string(name)}, .why = "TODO why"{extra}}},'


def source(step):
    return " ".join([step["command"], *step["args"]])


class Draft:
    """Rows and moves built up one step at a time."""

    def __init__(self, first_id):
        major, minor = first_id.split(".")
        self.major = major
        self.minor = int(minor)
        self.rows = [[]]
        self.moves = []
        self.raw = []
        self.face = None

    def line(self, text, step=None):
        self.flush_raw()
        comment = f"  // L{step['line']} {source(step)}" if step else ""
        self.rows[-1].append(text + comment)

    def review(self, step, why):
        self.line(f"// REVIEW L{step['line']} {source(step)}: {why}")

    def flush_raw(self):
        if not self.raw:
            return
        raw, self.raw = self.raw, []
        inputs = "; ".join(source(step) for step in raw)
        span = f"L{raw[0]['line']}-L{raw[-1]['line']}" if len(raw) > 1 else f"L{raw[0]['line']}"
        self.rows[-1].append(f"// RAW {len(raw)} inputs ({span}: {inputs}) -- REVIEW: replace with a story action")

    def flush_face(self):
        if self.face is not None:
            face, self.face = self.face, None
            self.review(face, "a turn on its own; fold it into the next lza_move's heading")

    def add(self, step):
        command = step["command"]
        if command in READ_COMMANDS:
            return
        if not step["ok"]:
            self.flush_face()
            self.review(step, f"FAILED: {step.get('error', '')}")
            return
        if command in RAW_COMMANDS:
            self.flush_face()
            self.raw.append(step)
            return
        if command == "lza_face":
            self.flush_face()
            self.face = step
            return
        if command == "lza_move":
            self.add_move(step)
            return
        self.flush_face()
        self.add_action(step)

    def add_move(self, step):
        heading, duration = step["args"]
        if heading == "ahead" and self.face is not None:
            heading = self.face["args"][0]
        self.face = None
        name = f"move_L{step['line']}"
        heading_cpp = "std::nullopt" if heading == "ahead" else heading
        self.moves.append(f"{{{cpp_string(name)}, StoryMove{{.heading = {heading_cpp}, .duration = {duration}ms}}}},")
        self.line(f'move({cpp_string(name)}, "TODO why"),', step)

    def add_action(self, step):
        command, args, name_line = step["command"], step["args"], f"_L{step['line']}"
        if command in SIMPLE_STEPS:
            kind, name = SIMPLE_STEPS[command]
            self.line(make(kind, name + name_line), step)
        elif command == "lza_talk":
            self.line(make("MASH", "scene" + name_line, "a" if args == ["A"] else None), step)
        elif command == "lza_pick":
            self.add_pick(step)
        elif command == "lza_fly":
            fields = [("target", f"in({cpp_string(args[0])})"), ("variant", '"fly"')]
            self.line(designated("ROUTE", "fly" + name_line, fields), step)
        elif command == "lza_shop":
            items = ", ".join(f"{{.item = {cpp_string(item)}, .quantity = 1}}" for item in args)
            self.line(designated("SHOP", "shop" + name_line, [("items", f"{{{items}}}")]), step)
        elif command == "lza_save":
            self.flush_raw()
            self.rows.append([])
        else:
            self.review(step, "no story step for this command")

    def add_pick(self, step):
        choice = step["args"][0]
        if choice not in ("0", "1"):
            self.review(step, "ANSWER picks choice 0 or 1; a longer list needs a GAME step")
            return
        fields = [("yes", "true" if choice == "0" else "false")]
        self.line(designated("ANSWER", f"answer_L{step['line']}", fields), step)

    def render(self, source_name):
        self.flush_face()
        self.flush_raw()
        rows = [row for row in self.rows if row]
        out = [f"//  Draft from {source_name}: {len(rows)} rows. Fill in every TODO and REVIEW."]
        for index, steps in enumerate(rows):
            row_id = f"{self.major}.{self.minor + index:02d}"
            out.append("rows.push_back(row(")
            out.append(f'    "{row_id}", "TODO title", in(LUMIOSE),')
            out.append("    {")
            out.extend(f"        {text}" for text in steps)
            out.append("    }")
            out.append("));")
        if self.moves:
            out.append("")
            out.append("//  story_moves():")
            out.extend(self.moves)
        return "\n".join(out) + "\n"


def convert(steps, source_name, first_id="01.01"):
    """The draft rows for `steps`, as C++ text."""
    draft = Draft(first_id)
    for step in steps:
        draft.add(step)
    return draft.render(source_name)


def main(argv):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("steps", help="a <batch>.steps.jsonl from the lab channel's outbox")
    parser.add_argument("--first-id", default="01.01", help="the first row's id, e.g. 01.03")
    args = parser.parse_args(argv)
    sys.stdout.write(convert(read_steps(args.steps), Path(args.steps).name, args.first_id))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
