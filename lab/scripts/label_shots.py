"""Label a directory of lab screenshots for detector training/test sets via the Message Batches API.

    python3 lab/scripts/label_shots.py SHOT_DIR --game bdsp --out labels.jsonl [--dry-run]
    python3 lab/scripts/label_shots.py SHOT_DIR --game bdsp --out labels.jsonl --resume msgbatch_...

Batches cost 50% of the normal price. Each shot is downscaled to <=640 px (lab_claude). Results are
keyed by custom_id (a hash of the shot's path relative to SHOT_DIR), never by position, so --resume
rebuilds the mapping from the directory. Shots already in the output JSONL are skipped.
"""

import argparse
import hashlib
import json
import sys
import time
from pathlib import Path

import lab_claude

DEFAULT_MODEL = "claude-haiku-5-5"
SCREENS = ["overworld", "battle", "dialog", "menu", "home", "fade_black", "color_bars", "other"]
GAMES = {"bdsp": "Pokemon Brilliant Diamond / Shining Pearl",
         "frlg": "Pokemon FireRed / LeafGreen",
         "plza": "Pokemon Legends: Z-A"}
SCHEMA = {
    "type": "object",
    "properties": {
        "screen": {"type": "string", "enum": SCREENS},
        "player_visible": {"type": "boolean"},
        "text_box_visible": {"type": "boolean"},
        "note": {"type": "string"},
    },
    "required": ["screen", "player_visible", "text_box_visible", "note"],
    "additionalProperties": False,
}
IMAGE_SUFFIXES = {".png", ".jpg", ".jpeg"}


def custom_id(rel_path):
    return "s" + hashlib.sha1(rel_path.encode("utf-8")).hexdigest()[:40]


def list_shots(shot_dir):
    shot_dir = Path(shot_dir)
    return sorted(p for p in shot_dir.rglob("*") if p.suffix.lower() in IMAGE_SUFFIXES and p.is_file())


def already_labeled(out_path):
    done = set()
    out_path = Path(out_path)
    if out_path.exists():
        for line in out_path.read_text().splitlines():
            if line.strip():
                done.add(json.loads(line)["path"])
    return done


def prompt(game):
    return (f"This is a Nintendo Switch capture from {GAMES[game]}, taken by an automation lab. "
            "Classify the screen: overworld (player walking around), battle, dialog (a story or NPC "
            "text box over the scene), menu (a game menu, bag, party, PC, map), home (the Switch HOME "
            "menu), fade_black (all or mostly black transition), color_bars (capture card test "
            "pattern / no signal), other. Say whether the player character is visible and whether a "
            "text box is visible. note: one short sentence on what is on screen.")


def build_request(path, rel_path, game, model):
    png = lab_claude.downscale_png(path)
    return {
        "custom_id": custom_id(rel_path),
        "params": {
            "model": model,
            "max_tokens": 1024,
            "output_config": {**lab_claude.json_format(SCHEMA), "effort": "low"},
            "messages": [{"role": "user", "content": [lab_claude.image_block(png),
                                                      {"type": "text", "text": prompt(game)}]}],
        },
    }


def wait_for_batch(client, batch_id, max_minutes, poll_seconds=30, sleep=time.sleep):
    deadline = time.monotonic() + max_minutes * 60
    while True:
        batch = client.messages.batches.retrieve(batch_id)
        if batch.processing_status == "ended":
            return True
        if time.monotonic() >= deadline:
            return False
        print(f"{batch_id}: {batch.processing_status}, processing={batch.request_counts.processing}",
              file=sys.stderr)
        sleep(poll_seconds)


def collect(client, batch_id, id_to_path, out_path, model):
    """Append succeeded results to out_path; return (written, failed)."""
    written, failed = 0, 0
    with open(out_path, "a") as out:
        for result in client.messages.batches.results(batch_id):
            path = id_to_path.get(result.custom_id)
            if path is None:
                print(f"warning: unknown custom_id {result.custom_id}", file=sys.stderr)
                continue
            if result.result.type != "succeeded":
                print(f"{path}: {result.result.type}", file=sys.stderr)
                failed += 1
                continue
            message = result.result.message
            if message.stop_reason in ("refusal", "max_tokens"):
                print(f"{path}: stop_reason {message.stop_reason}", file=sys.stderr)
                failed += 1
                continue
            text = next(b.text for b in message.content if b.type == "text")
            out.write(json.dumps({"path": path, "label": json.loads(text), "model": model}) + "\n")
            written += 1
    return written, failed


def main(argv=None, client=None):
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("shot_dir")
    parser.add_argument("--game", required=True, choices=sorted(GAMES))
    parser.add_argument("--out", required=True, help="output JSONL {path, label, model}")
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--max-minutes", type=float, default=60, help="poll cap; then --resume later")
    parser.add_argument("--resume", metavar="BATCH_ID", help="collect an earlier batch instead of submitting")
    parser.add_argument("--dry-run", action="store_true", help="print the first request and the count")
    args = parser.parse_args(argv)

    shot_dir = Path(args.shot_dir)
    done = already_labeled(args.out)
    todo = []
    for shot in list_shots(shot_dir):
        rel = shot.relative_to(shot_dir).as_posix()
        if str(shot) not in done:
            todo.append((shot, rel))
    id_to_path = {custom_id(rel): str(shot) for shot, rel in todo}

    if args.dry_run:
        print(f"{len(todo)} shots to label ({len(done)} already in {args.out})")
        if todo:
            lab_claude.dry_run_print(build_request(todo[0][0], todo[0][1], args.game, args.model))
        return 0
    if not todo:
        print("nothing to label")
        return 0

    client = client or lab_claude.make_client()
    batch_id = args.resume
    if batch_id is None:
        requests = [build_request(shot, rel, args.game, args.model) for shot, rel in todo]
        batch_id = client.messages.batches.create(requests=requests).id
        print(f"submitted {batch_id} with {len(requests)} shots")
    if not wait_for_batch(client, batch_id, args.max_minutes):
        print(f"not finished after {args.max_minutes} min; collect later with --resume {batch_id}")
        return 2
    written, failed = collect(client, batch_id, id_to_path, args.out, args.model)
    print(f"{batch_id}: wrote {written} labels to {args.out}, {failed} failed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
