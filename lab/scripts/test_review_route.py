"""Offline tests with a mocked client: python3 -m unittest discover -s lab/scripts -p test_review_route.py -v."""

import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock

import review_route

REVIEW = {
    "legs": [{"leg_id": "1", "kind": "open_run", "risk": "high", "reason": "12 tiles, no wall | stop"},
             {"leg_id": "2", "kind": "wall_clip", "risk": "low", "reason": "runs into north wall"}],
    "totals": {"unprotected_moves": 1, "localizations": 1, "turns": 2},
    "suggestions": ["Leg 1: end against the fence instead."],
}


def fake_client(payload, stop_reason="end_turn"):
    response = SimpleNamespace(stop_reason=stop_reason, stop_details=None,
                               content=[SimpleNamespace(type="text", text=json.dumps(payload))])
    client = MagicMock()
    client.beta.messages.create.return_value = response
    return client


class ReviewRouteTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = Path(tmp.name)
        self.plan = self.dir / "bdsp-00.04.plan.json"
        self.plan.write_text('{"legs": [{"id": 1}]}', encoding="utf-8")

    def test_request_shape(self):
        request = review_route.build_request("{}", "map text", "sim text")
        self.assertEqual(request["model"], "claude-opus-5-5")
        self.assertEqual(request["thinking"], {"type": "adaptive"})
        self.assertEqual(request["output_config"]["effort"], "high")
        self.assertEqual(request["output_config"]["format"]["type"], "json_schema")
        self.assertEqual(request["fallbacks"], "default")
        self.assertEqual(request["betas"], ["server-side-fallback-2026-07-01"])
        text = request["messages"][0]["content"][0]["text"]
        self.assertIn("<map>", text)
        self.assertIn("<sim>", text)
        for phrase in ("unprotected", "localization", "turns", "funnel", "Long open legs", "guess layout"):
            self.assertIn(phrase, request["system"])

    def test_optional_inputs_omitted(self):
        text = review_route.build_request("{}")["messages"][0]["content"][0]["text"]
        self.assertNotIn("<map>", text)
        self.assertNotIn("<sim>", text)

    def test_dry_run_needs_no_client(self):
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(review_route.main(["--plan", str(self.plan), "--dry-run"]), 0)
        self.assertEqual(json.loads(out.getvalue())["fallbacks"], "default")

    def test_notes_required_without_dry_run(self):
        with self.assertRaises(SystemExit):
            review_route.parse_args(["--plan", str(self.plan)])

    def test_appends_block(self):
        notes = self.dir / "notes" / "routes.md"
        client = fake_client(REVIEW)
        with redirect_stdout(io.StringIO()):
            review_route.main(["--plan", str(self.plan), "--notes", str(notes)], client=client)
        client.beta.messages.create.assert_called_once()
        body = notes.read_text(encoding="utf-8")
        self.assertIn("### Route review bdsp-00.04.plan.json", body)
        self.assertIn("unprotected_moves 1, localizations 1, turns 2", body)
        self.assertIn("| 1 | open_run | high | 12 tiles, no wall \\| stop |", body)
        self.assertIn("  - Leg 1: end against the fence instead.", body)

    def test_refusal_raises(self):
        with self.assertRaises(RuntimeError):
            review_route.main(["--plan", str(self.plan), "--notes", str(self.dir / "n.md")],
                              client=fake_client(REVIEW, stop_reason="refusal"))


if __name__ == "__main__":
    unittest.main()
