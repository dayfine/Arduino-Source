"""Offline tests of cfr_steps_to_story.py on a recorded sample."""

import json
from pathlib import Path
import unittest

from cfr_steps_to_story import convert, read_steps


TESTDATA = Path(__file__).parent / "testdata"


def step(line, command, *args, ok=True):
    """A steps-file line with only the fields the converter reads."""
    return {"line": line, "command": command, "args": list(args), "ok": ok}


class SampleTests(unittest.TestCase):
    def test_sample_converts_to_the_reviewed_draft(self):
        steps = read_steps(TESTDATA / "cfr_steps_sample.steps.jsonl")
        expected = (TESTDATA / "cfr_steps_sample.expected.cpp").read_text(encoding="utf-8")
        self.assertEqual(expected, convert(steps, "cfr_steps_sample.steps.jsonl", "01.03"))

    def test_sample_lines_are_recorder_lines(self):
        for line in (TESTDATA / "cfr_steps_sample.steps.jsonl").read_text(encoding="utf-8").splitlines():
            fields = set(json.loads(line))
            self.assertLessEqual({"line", "command", "args", "start", "end", "duration_ms", "ok", "screenshot"}, fields)


class RuleTests(unittest.TestCase):
    def test_raw_inputs_collapse_into_one_gap(self):
        draft = convert([step(1, "press", "A"), step(2, "dpad", "UP"), step(3, "stick", "L", "0", "1")], "s")
        self.assertIn("// RAW 3 inputs (L1-L3: press A; dpad UP; stick L 0 1) -- REVIEW", draft)
        self.assertEqual(1, draft.count("RAW"))

    def test_reads_are_left_out(self):
        draft = convert([step(1, "shot", "x"), step(2, "detect", "lza_dialog"), step(3, "lza_battle")], "s")
        self.assertNotIn("shot", draft)
        self.assertNotIn("detect", draft)
        self.assertIn('make(StoryStepKind::BATTLE, "battle_L3", "TODO why"),', draft)

    def test_turn_then_move_ahead_is_one_move(self):
        draft = convert([step(1, "lza_face", "180"), step(2, "lza_move", "ahead", "900")], "s")
        self.assertIn('{"move_L2", StoryMove{.heading = 180, .duration = 900ms}},', draft)
        self.assertNotIn("// REVIEW", draft)

    def test_move_ahead_has_no_heading(self):
        draft = convert([step(1, "lza_move", "ahead", "900")], "s")
        self.assertIn('{"move_L1", StoryMove{.heading = std::nullopt, .duration = 900ms}},', draft)

    def test_save_starts_the_next_row(self):
        draft = convert([step(1, "lza_battle"), step(2, "lza_save"), step(3, "lza_heal")], "s", "02.05")
        self.assertIn('"02.05", "TODO title"', draft)
        self.assertIn('"02.06", "TODO title"', draft)

    def test_failed_command_is_flagged_not_a_step(self):
        draft = convert([dict(step(4, "lza_fly", "hotel-z", ok=False), error="no fly")], "s")
        self.assertIn("// REVIEW L4 lza_fly hotel-z: FAILED: no fly", draft)
        self.assertNotIn("ROUTE", draft)


if __name__ == "__main__":
    unittest.main()
