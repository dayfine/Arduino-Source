#!/usr/bin/env python3
"""Classify one lab console screenshot with Claude Haiku for the overnight check-in.

Prints the verdict JSON. Exit 0 = feed alive and in game; 2 = dead feed, color bars or HOME;
1 = error. Example: python3 lab/scripts/watchdog_shot.py shot.png [--dry-run]
"""

import argparse
import json
import sys

import lab_claude

MODEL = "claude-haiku-5-5"
SCREENS = ["overworld", "battle", "dialog", "menu", "home", "color_bars", "black", "frozen_unknown"]
ALERT_SCREENS = {"home", "color_bars"}
EXIT_ALERT = 2

SCHEMA = {
    "type": "object",
    "properties": {
        "feed_alive": {"type": "boolean"},
        "screen": {"type": "string", "enum": SCREENS},
        "stuck_suspected": {"type": "boolean"},
        "note": {"type": "string"},
    },
    "required": ["feed_alive", "screen", "stuck_suspected", "note"],
    "additionalProperties": False,
}

PROMPT = """This is one capture-card frame from a Nintendo Switch running a Pokemon game.
Classify only what is visible. color_bars = the capture card's test pattern (no signal);
home = the Switch HOME menu; black = an all-black frame. feed_alive is false for color bars or
a frame with no game picture. Keep note to one short sentence describing what is on screen."""


def build_request(shot):
    return {
        "model": MODEL,
        "max_tokens": 1024,
        "output_config": {"effort": "low", **lab_claude.json_format(SCHEMA)},
        "messages": [{"role": "user", "content": [
            lab_claude.image_block(lab_claude.downscale_png(shot)),
            {"type": "text", "text": PROMPT}]}],
    }


def exit_code(verdict):
    if not verdict["feed_alive"] or verdict["screen"] in ALERT_SCREENS:
        return EXIT_ALERT
    return 0


def main(argv=None, client=None):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("shot", help="screenshot PNG")
    parser.add_argument("--dry-run", action="store_true", help="print the request; no network")
    args = parser.parse_args(argv)
    request = build_request(args.shot)
    if args.dry_run:
        lab_claude.dry_run_print(request)
        return 0
    try:
        verdict = lab_claude.send(request, client=client)
    except Exception as error:  # report and let the caller branch on 1
        print(f"watchdog error: {error}", file=sys.stderr)
        return 1
    print(json.dumps(verdict))
    return exit_code(verdict)


if __name__ == "__main__":
    sys.exit(main())
