#!/usr/bin/env python3
"""Ask Claude to review one AutoStory checkpoint plan before coding it; append the review to notes.

Example:
  python3 lab/scripts/review_route.py --plan ~/Projects/wt/x/StoryData/bdsp-00.04.plan.json \
      --notes ~/BDSPLab/notes/2026-11-11-routes.md [--map-slice slice.txt] [--sim sim.txt] [--dry-run]
"""

import argparse
import datetime
import sys
from pathlib import Path

import lab_claude

MODEL = "claude-opus-5-5"
LEG_KINDS = ["open_run", "wall_clip", "tap", "door", "talk", "warp", "battle", "menu", "other"]
RISKS = ["low", "med", "high"]

SCHEMA = {
    "type": "object",
    "properties": {
        "legs": {"type": "array", "items": {
            "type": "object",
            "properties": {
                "leg_id": {"type": "string"},
                "kind": {"type": "string", "enum": LEG_KINDS},
                "risk": {"type": "string", "enum": RISKS},
                "reason": {"type": "string"},
            },
            "required": ["leg_id", "kind", "risk", "reason"],
            "additionalProperties": False,
        }},
        "totals": {
            "type": "object",
            "properties": {
                "unprotected_moves": {"type": "integer"},
                "localizations": {"type": "integer"},
                "turns": {"type": "integer"},
            },
            "required": ["unprotected_moves", "localizations", "turns"],
            "additionalProperties": False,
        },
        "suggestions": {"type": "array", "items": {"type": "string"}},
    },
    "required": ["legs", "totals", "suggestions"],
    "additionalProperties": False,
}

SYSTEM = """You review the route plan of one AutoStory checkpoint (a Pokemon story run on a Nintendo
Switch, driven by timed stick and D-pad inputs) before anyone writes code for it.

Score the route with the user's preferences:
- Few unprotected moves, each with a wide margin. An unprotected move is one whose end position
  depends on timing alone, with no wall, ledge or funnel to stop it.
- One localization per checkpoint is ideal: running into a wall or funnel to re-localize is
  preferred over counting steps.
- Few turns.
- Long open legs are the main risk; a hold-run open leg is fine only when overshoot is harmless.

Classify every leg in plan order (leg_id = the plan's id or index), give a risk and a one-sentence
reason, then count totals and give concrete suggestions (which leg, what to change).

Report only what the plan, map slice and sim output literally show. Do not guess layout: if a
wall, door or funnel is not in the given text, say what would confirm it instead of assuming it."""


def build_request(plan_text, map_slice=None, sim_output=None):
    text = ["Plan (JSON):", "<plan>", plan_text, "</plan>"]
    if map_slice:
        text += ["Map slice (text; do not infer beyond what it literally says):", "<map>", map_slice, "</map>"]
    if sim_output:
        text += ["Simulator output:", "<sim>", sim_output, "</sim>"]
    return {
        "model": MODEL,
        "max_tokens": 16000,
        "thinking": {"type": "adaptive"},
        "output_config": {"effort": "high", **lab_claude.json_format(SCHEMA)},
        "betas": [lab_claude.FALLBACK_BETA],
        "fallbacks": "default",
        "system": SYSTEM,
        "messages": [{"role": "user", "content": [{"type": "text", "text": "\n".join(text)}]}],
    }


def format_block(review, plan_name, now=None):
    now = now or datetime.datetime.now()
    totals = review["totals"]
    lines = [f"### Route review {plan_name} ({now:%Y-%m-%d %H:%M}, {MODEL})", "",
             f"- totals: unprotected_moves {totals['unprotected_moves']}, "
             f"localizations {totals['localizations']}, turns {totals['turns']}", "",
             "| leg | kind | risk | reason |", "|---|---|---|---|"]
    for leg in review["legs"]:
        reason = leg["reason"].replace("|", "\\|").replace("\n", " ")
        lines.append(f"| {leg['leg_id']} | {leg['kind']} | {leg['risk']} | {reason} |")
    lines += ["", "- suggestions:"]
    lines += [f"  - {item}" for item in review["suggestions"]] or ["  - (none)"]
    return "\n".join(lines) + "\n"


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--plan", required=True, help="checkpoint plan JSON (e.g. StoryData/bdsp-*.plan.json)")
    parser.add_argument("--map-slice", help="optional map/grid text slice")
    parser.add_argument("--sim", help="optional simulator output text file")
    parser.add_argument("--notes", help="notes file to append to (required unless --dry-run)")
    parser.add_argument("--dry-run", action="store_true", help="print the request; no network")
    args = parser.parse_args(argv)
    if not args.dry_run and not args.notes:
        parser.error("--notes is required unless --dry-run")
    return args


def read_optional(path):
    return Path(path).read_text(encoding="utf-8") if path else None


def main(argv=None, client=None):
    args = parse_args(argv)
    plan = Path(args.plan)
    request = build_request(plan.read_text(encoding="utf-8"), read_optional(args.map_slice),
                            read_optional(args.sim))
    if args.dry_run:
        lab_claude.dry_run_print(request)
        return 0
    review = lab_claude.send(request, client=client, beta=True)
    block = format_block(review, plan.name)
    notes = Path(args.notes).expanduser()
    notes.parent.mkdir(parents=True, exist_ok=True)
    with open(notes, "a", encoding="utf-8") as handle:
        handle.write("\n" + block)
    print(block)
    return 0


if __name__ == "__main__":
    sys.exit(main())
