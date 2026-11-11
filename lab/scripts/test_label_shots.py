"""Offline tests with a mocked client: python3 -m unittest discover -s lab/scripts -p test_label_shots.py -v."""

import datetime
import io
import json
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock

import lab_claude
import label_shots
from test_lab_claude import tiny_png


def label(screen):
    return {"screen": screen, "player_visible": True, "text_box_visible": False, "note": screen}


def result(cid, payload):
    message = SimpleNamespace(stop_reason="end_turn",
                              usage=SimpleNamespace(input_tokens=1_000_000, output_tokens=0),
                              content=[SimpleNamespace(type="text", text=json.dumps(payload))])
    return SimpleNamespace(custom_id=cid, result=SimpleNamespace(type="succeeded", message=message))


class LabelShotsTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.dir = Path(tmp.name) / "shots"
        (self.dir / "sub").mkdir(parents=True)
        for name in ["a.png", "b.png", "sub/c.png"]:
            (self.dir / name).write_bytes(tiny_png())
        (self.dir / "notes.txt").write_text("x")
        self.out = Path(tmp.name) / "labels.jsonl"
        self.ledger = Path(tmp.name) / "spend.jsonl"
        old_ledger = lab_claude.LEDGER
        lab_claude.LEDGER = self.ledger
        self.addCleanup(setattr, lab_claude, "LEDGER", old_ledger)

    def args(self, *extra):
        return [str(self.dir), "--game", "bdsp", "--out", str(self.out), *extra]

    def client(self, results):
        client = MagicMock()
        client.messages.batches.create.return_value = SimpleNamespace(id="msgbatch_1")
        client.messages.batches.retrieve.return_value = SimpleNamespace(processing_status="ended")
        client.messages.batches.results.return_value = iter(results)
        return client

    def test_request_shape(self):
        request = label_shots.build_request(self.dir / "a.png", "a.png", "frlg", "claude-haiku-5-5")
        self.assertEqual(request["custom_id"], label_shots.custom_id("a.png"))
        self.assertRegex(request["custom_id"], r"^[A-Za-z0-9_-]{1,64}$")
        params = request["params"]
        self.assertEqual(params["model"], "claude-haiku-5-5")
        fmt = params["output_config"]["format"]
        self.assertEqual(fmt["type"], "json_schema")
        self.assertEqual(fmt["schema"]["properties"]["screen"]["enum"], label_shots.SCREENS)
        content = params["messages"][0]["content"]
        self.assertEqual(content[0]["type"], "image")
        self.assertIn("FireRed", content[1]["text"])

    def test_results_keyed_by_custom_id_not_position(self):
        rels = ["a.png", "b.png", "sub/c.png"]
        screens = {"a.png": "battle", "b.png": "menu", "sub/c.png": "home"}
        results = [result(label_shots.custom_id(r), label(screens[r])) for r in reversed(rels)]
        client = self.client(results)
        self.assertEqual(label_shots.main(self.args(), client=client), 0)
        rows = [json.loads(line) for line in self.out.read_text().splitlines()]
        self.assertEqual(len(rows), 3)
        for row in rows:
            rel = Path(row["path"]).relative_to(self.dir).as_posix()
            self.assertEqual(row["label"]["screen"], screens[rel])
            self.assertEqual(row["model"], "claude-haiku-5-5")
        sent = client.messages.batches.create.call_args.kwargs["requests"]
        self.assertEqual(len(sent), 3)

    def test_skips_shots_already_labeled(self):
        self.out.write_text(json.dumps({"path": str(self.dir / "a.png"), "label": label("battle"),
                                        "model": "m"}) + "\n")
        client = self.client([result(label_shots.custom_id("b.png"), label("menu"))])
        label_shots.main(self.args(), client=client)
        sent = client.messages.batches.create.call_args.kwargs["requests"]
        self.assertEqual({r["custom_id"] for r in sent},
                         {label_shots.custom_id("b.png"), label_shots.custom_id("sub/c.png")})
        self.assertEqual(len(self.out.read_text().splitlines()), 2)

    def test_budget_refusal_blocks_submit(self):
        today = datetime.date.today().isoformat()
        self.ledger.write_text(json.dumps({"date": today, "script": "x", "model": "m",
                                           "usd": lab_claude.DAILY_USD}) + "\n")
        client = self.client([])
        with redirect_stdout(io.StringIO()):
            self.assertEqual(label_shots.main(self.args(), client=client), 3)
        client.messages.batches.create.assert_not_called()

    def test_collected_results_recorded_at_half_price(self):
        client = self.client([result(label_shots.custom_id("a.png"), label("menu"))])
        with redirect_stdout(io.StringIO()):
            label_shots.main(self.args(), client=client)
        entries = [json.loads(line) for line in self.ledger.read_text().splitlines()]
        self.assertEqual(len(entries), 1)
        full = lab_claude.cost_usd(label_shots.DEFAULT_MODEL,
                                   SimpleNamespace(input_tokens=1_000_000, output_tokens=0))
        self.assertAlmostEqual(entries[0]["usd"], full / 2, places=5)

    def test_resume_collects_without_submitting(self):
        client = self.client([result(label_shots.custom_id("b.png"), label("menu"))])
        label_shots.main(self.args("--resume", "msgbatch_9"), client=client)
        client.messages.batches.create.assert_not_called()
        client.messages.batches.results.assert_called_once_with("msgbatch_9")

    def test_poll_cap_returns_resume_hint(self):
        client = self.client([])
        client.messages.batches.retrieve.return_value = SimpleNamespace(
            processing_status="in_progress", request_counts=SimpleNamespace(processing=3))
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(label_shots.main(self.args("--max-minutes", "0"), client=client), 2)
        self.assertIn("--resume msgbatch_1", out.getvalue())
        self.assertFalse(self.out.exists())

    def test_dry_run_prints_first_request_and_count(self):
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(label_shots.main(self.args("--dry-run")), 0)
        text = out.getvalue()
        self.assertTrue(text.startswith("3 shots to label"))
        request = json.loads(text.split("\n", 1)[1])
        self.assertIn("base64 chars", request["params"]["messages"][0]["content"][0]["source"]["data"])


if __name__ == "__main__":
    unittest.main()
