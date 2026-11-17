import json
import tempfile
import unittest
from pathlib import Path

from devtools.plza_autosaves import promote

PLAN = """{
  "format": "pokesim-authored-checkpoint/1",
  "game": "plza",
  "checkpoint": {
    "id": "02.07",
    "events": [
      {
        "id": "to_room_door",
        "kind": "game_action"
      },
      {
        "id": "first_evening",
        "kind": "game_action"
      }
    ],
    "metadata": {
      "phase": "testing",
      "source": "console B recording. Save unverified: its run passed with no autosave badge in the log (console B, 2026-11-13 14:16)."
    }
  },
  "execution": {
    "save": "unverified"
  }
}
"""

ROW_STEPS = {"02.07": ["to_room_door", "first_evening"]}


def log(*lines):
    return "".join(f"2026-11-16 21:{i:02d}:00.000000 - [Console 0]: {line}\n" for i, line in enumerate(lines))


def sightings(text, row_steps=ROW_STEPS):
    return promote.read_sightings(text.splitlines(), row_steps)


class ReadSightingsTest(unittest.TestCase):
    def test_kinds(self):
        cases = [
            {
                "name": "badge in the last step of a passed run",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] autosave badge at 30.0s in first_evening",
                    "[Save] 02.07: the game autosaved (seen)",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [("02.07", "end", "first_evening")],
            },
            {
                "name": "a menu save alone is no autosave",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] saved from the menu in first_evening",
                    "[Save] 02.07: the game autosaved (seen)",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [("02.07", "none", "")],
            },
            {
                "name": "badge before the last step",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] autosave badge at 3.0s in to_room_door",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [("02.07", "mid", "to_room_door")],
            },
            {
                "name": "badge in a failed run",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] autosave badge at 30.0s in first_evening",
                    "[Checkpoint] 02.07: failed at 2 first_evening: operation_failed: x",
                ),
                "expectation": [("02.07", "failed", "first_evening")],
            },
            {
                "name": "older log line format",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[Autosave] 02.07/2 first_evening",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [("02.07", "end", "first_evening")],
            },
            {
                "name": "step not in the plan row: another game or an old plan",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] autosave badge at 30.0s in west_of_barrys_trigger",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [("02.07", "none", "")],
            },
            {
                "name": "a badge earlier in the run, the last sighting before the save",
                "input": log(
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] autosave badge at 3.0s in to_room_door",
                    "[Save] 02.07: the game autosaved (seen)",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [("02.07", "end", "first_evening")],
            },
            {
                "name": "another game's program with the same checkpoint id",
                "input": log(
                    "<b>Starting Program: PokemonBDSP:AutoStory</b>",
                    "[Checkpoint] 02.07: start, 2 events",
                    "[PLZA] autosave badge at 30.0s in first_evening",
                    "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                ),
                "expectation": [],
            },
            {
                "name": "badge outside any checkpoint",
                "input": log("[PLZA] autosave badge at 30.0s in first_evening"),
                "expectation": [],
            },
        ]
        for case in cases:
            with self.subTest(case["name"]):
                got = [(s.row, s.kind, s.step) for s in sightings(case["input"])]
                self.assertEqual(got, case["expectation"])

    def test_sighting_keeps_the_log_time(self):
        found = sightings(log(
            "[Checkpoint] 02.07: start, 2 events",
            "[PLZA] autosave badge at 30.0s in first_evening",
            "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
        ))
        self.assertEqual(found[0].when, "2026-11-16 21:01:00")


class PromoteTextTest(unittest.TestCase):
    def test_save_becomes_autosave_with_its_step(self):
        text = promote.promote_text(PLAN, step="first_evening", when="2026-11-16 21:01:00")
        execution = json.loads(text)["execution"]
        self.assertEqual(execution, {"save": "autosave", "autosave_step": "first_evening"})

    def test_unverified_sentence_cites_the_log(self):
        text = promote.promote_text(PLAN, step="first_evening", when="2026-11-16 21:01:00")
        source = json.loads(text)["checkpoint"]["metadata"]["source"]
        self.assertEqual(
            source,
            "console B recording. Autosave: a run log showed the badge in first_evening, its last step, "
            "and the checkpoint passed (console B, 2026-11-16 21:01:00).",
        )

    def test_source_without_unverified_gets_the_citation_appended(self):
        plan = PLAN.replace(
            " Save unverified: its run passed with no autosave badge in the log (console B, 2026-11-13 14:16).", ""
        )
        text = promote.promote_text(plan, step="first_evening", when="2026-11-16 21:01:00")
        source = json.loads(text)["checkpoint"]["metadata"]["source"]
        self.assertTrue(source.startswith("console B recording. Autosave: a run log showed"))

    def test_other_lines_and_crlf_endings_stay(self):
        crlf = PLAN.replace("\n", "\r\n")
        text = promote.promote_text(crlf, step="first_evening", when="2026-11-16 21:01:00")
        self.assertNotIn("\n", text.replace("\r\n", ""))
        changed = [line for line in text.split("\r\n") if line not in crlf.split("\r\n")]
        self.assertEqual(len(changed), 3)


class ApplyTest(unittest.TestCase):
    def test_apply_edits_only_unverified_rows_seen_at_their_end(self):
        with tempfile.TemporaryDirectory() as tmp:
            plan_dir = Path(tmp)
            (plan_dir / "plza-02.07.plan.json").write_bytes(PLAN.encode())
            log_file = plan_dir / "SerialPrograms.log"
            log_file.write_text(log(
                "[Checkpoint] 02.07: start, 2 events",
                "[PLZA] autosave badge at 30.0s in first_evening",
                "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
            ))
            report = promote.run([log_file], plan_dir, apply=True)
            execution = json.loads((plan_dir / "plza-02.07.plan.json").read_text())["execution"]
            self.assertEqual(execution["save"], "autosave")
            self.assertIn("promoted 02.07", report)

    def test_a_passed_run_without_the_badge_blocks_the_promotion(self):
        with tempfile.TemporaryDirectory() as tmp:
            plan_dir = Path(tmp)
            (plan_dir / "plza-02.07.plan.json").write_bytes(PLAN.encode())
            log_file = plan_dir / "SerialPrograms.log"
            log_file.write_text(log(
                "[Checkpoint] 02.07: start, 2 events",
                "[PLZA] autosave badge at 30.0s in first_evening",
                "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
                "[Checkpoint] 02.07: start, 2 events",
                "[Checkpoint] 02.07: passed on attempt 1 (one-shot)",
            ))
            promote.run([log_file], plan_dir, apply=True)
            execution = json.loads((plan_dir / "plza-02.07.plan.json").read_text())["execution"]
            self.assertEqual(execution["save"], "unverified")


if __name__ == "__main__":
    unittest.main()
