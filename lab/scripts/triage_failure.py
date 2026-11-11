#!/usr/bin/env python3
"""Ask Claude to classify a lab console failure and append the verdict to a notes file.

Example:
  python3 lab/scripts/triage_failure.py --frame fail.png --checkpoint 00.04 \
      --step step.json --notes ~/BDSPLab/notes/2026-11-11-checklist.md [--map-slice slice.txt] [--dry-run]
"""

import argparse
import datetime
import json
import sys
from pathlib import Path

import lab_claude

MODEL = "claude-opus-5-5"
DEFAULT_LOG = Path.home() / "Library/Application Support/SerialPrograms/Profiles/lab-B/SerialPrograms.log"
FAULT_CLASSES = ["wrong_position", "unexpected_battle", "dialog_or_menu", "map_error", "timing",
                 "dead_feed", "controller_disconnected", "other"]

SCHEMA = {
    "type": "object",
    "properties": {
        "fault_class": {"type": "string", "enum": FAULT_CLASSES},
        "failing_step": {"type": "string"},
        "evidence": {"type": "array", "items": {"type": "string"}},
        "confidence": {"type": "string", "enum": ["low", "medium", "high"]},
        "suggested_next_check": {"type": "string"},
    },
    "required": ["fault_class", "failing_step", "evidence", "confidence", "suggested_next_check"],
    "additionalProperties": False,
}

SYSTEM = """You triage failures of a Nintendo Switch automation program (Pokemon story runs) on a lab console.
You get the failure frame(s), the tail of the program log, the checkpoint id, the plan step that was
running, and sometimes a text slice of the navigation map.

Report what is seen, not what the layout might be. Every evidence item must cite a specific log line
(quote it) or a concrete visible feature of a frame (where on screen, what it shows). Do not infer
map layout, tile meaning or trainer positions from the map slice or the frame; if the cause depends
on layout, say what would confirm it in suggested_next_check instead. Past mistakes to avoid: a lift
exit tile was mislabeled as a trainer battle, and a map slice was misread. When the evidence is thin,
use fault_class "other" or low confidence rather than guessing."""


def tail_lines(path, count):
    try:
        with open(path, encoding="utf-8", errors="replace") as handle:
            return "".join(handle.readlines()[-count:])
    except OSError as error:
        return f"(log not readable: {error})"


def build_request(frames, log_text, checkpoint, step_json, map_slice):
    content = [lab_claude.image_block(lab_claude.downscale_png(frame)) for frame in frames]
    text = [f"Checkpoint: {checkpoint}", "Plan step (JSON):", step_json,
            "Log tail:", "<log>", log_text, "</log>"]
    if map_slice:
        text += ["Map slice (text; do not infer layout beyond what it literally says):", map_slice]
    content.append({"type": "text", "text": "\n".join(text)})
    return {
        "model": MODEL,
        "max_tokens": 16000,
        "thinking": {"type": "adaptive"},
        "output_config": {"effort": "high", **lab_claude.json_format(SCHEMA)},
        "betas": [lab_claude.FALLBACK_BETA],
        "fallbacks": "default",
        "system": SYSTEM,
        "messages": [{"role": "user", "content": content}],
    }


def format_block(verdict, checkpoint, now=None):
    now = now or datetime.datetime.now()
    lines = [f"### Triage {checkpoint} ({now:%Y-%m-%d %H:%M}, {MODEL})", "",
             f"- fault_class: `{verdict['fault_class']}` (confidence {verdict['confidence']})",
             f"- failing_step: {verdict['failing_step']}", "- evidence:"]
    lines += [f"  - {item}" for item in verdict["evidence"]]
    lines += [f"- next check: {verdict['suggested_next_check']}", ""]
    return "\n".join(lines) + "\n"


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--frame", action="append", required=True, help="failure frame PNG (repeatable)")
    parser.add_argument("--checkpoint", required=True)
    parser.add_argument("--step", required=True, help="plan step JSON file")
    parser.add_argument("--notes", help="notes file to append to (required unless --dry-run)")
    parser.add_argument("--log", default=str(DEFAULT_LOG))
    parser.add_argument("--log-lines", type=int, default=200)
    parser.add_argument("--map-slice", help="optional map slice text file")
    parser.add_argument("--dry-run", action="store_true", help="print the request; no network")
    args = parser.parse_args(argv)
    if not args.dry_run and not args.notes:
        parser.error("--notes is required unless --dry-run")
    return args


def main(argv=None, client=None):
    args = parse_args(argv)
    step_json = Path(args.step).read_text(encoding="utf-8")
    map_slice = Path(args.map_slice).read_text(encoding="utf-8") if args.map_slice else None
    request = build_request(args.frame, tail_lines(args.log, args.log_lines), args.checkpoint,
                            step_json, map_slice)
    if args.dry_run:
        lab_claude.dry_run_print(request)
        return 0
    verdict = lab_claude.send(request, client=client, beta=True)
    block = format_block(verdict, args.checkpoint)
    notes = Path(args.notes).expanduser()
    notes.parent.mkdir(parents=True, exist_ok=True)
    with open(notes, "a", encoding="utf-8") as handle:
        handle.write("\n" + block)
    print(block)
    return 0


if __name__ == "__main__":
    sys.exit(main())
