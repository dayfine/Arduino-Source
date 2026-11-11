"""Offline tests with a mocked client: python3 -m unittest discover -s lab/scripts -p test_lab_claude.py -v."""

import io
import json
import struct
import tempfile
import unittest
import zlib
from contextlib import redirect_stdout
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import MagicMock

import triage_failure
import watchdog_shot


def tiny_png():
    raw = b"\x00\x00\x00\x00"
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", 1, 1, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw)) + chunk(b"IEND", b""))


def fake_client(payload, stop_reason="end_turn"):
    response = SimpleNamespace(stop_reason=stop_reason, stop_details=None,
                               content=[SimpleNamespace(type="text", text=json.dumps(payload))])
    client = MagicMock()
    client.messages.create.return_value = response
    client.beta.messages.create.return_value = response
    return client


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)
        self.png = self.dir / "f.png"
        self.png.write_bytes(tiny_png())


class TriageTests(Base):
    VERDICT = {"fault_class": "timing", "failing_step": "walk 3", "evidence": ["log: 'timeout'"],
               "confidence": "medium", "suggested_next_check": "rerun with a shot"}

    def args(self, *extra):
        step = self.dir / "step.json"
        step.write_text('{"kind": "walk"}')
        log = self.dir / "log.txt"
        log.write_text("".join(f"line {i}\n" for i in range(300)))
        return ["--frame", str(self.png), "--checkpoint", "00.04", "--step", str(step),
                "--log", str(log), *extra]

    def test_dry_run_prints_request_without_network(self):
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(triage_failure.main(self.args("--dry-run")), 0)
        request = json.loads(out.getvalue())
        self.assertEqual(request["model"], "claude-opus-5-5")
        self.assertEqual(request["thinking"], {"type": "adaptive"})
        self.assertEqual(request["output_config"]["effort"], "high")
        self.assertEqual(request["fallbacks"], "default")
        self.assertIn("server-side-fallback-2026-07-01", request["betas"])
        text = request["messages"][0]["content"][-1]["text"]
        self.assertIn("line 299", text)
        self.assertNotIn("line 99\n", text)
        self.assertIn("base64 chars", json.dumps(request))

    def test_appends_markdown_block(self):
        notes = self.dir / "notes" / "checklist.md"
        client = fake_client(self.VERDICT)
        with redirect_stdout(io.StringIO()):
            triage_failure.main(self.args("--notes", str(notes)), client=client)
        client.beta.messages.create.assert_called_once()
        body = notes.read_text()
        self.assertIn("fault_class: `timing`", body)
        self.assertIn("  - log: 'timeout'", body)

    def test_refusal_raises(self):
        client = fake_client({}, stop_reason="refusal")
        with self.assertRaises(RuntimeError):
            triage_failure.main(self.args("--notes", str(self.dir / "n.md")), client=client)


class WatchdogTests(Base):
    def run_with(self, payload):
        with redirect_stdout(io.StringIO()):
            return watchdog_shot.main([str(self.png)], client=fake_client(payload))

    def test_exit_codes(self):
        base = {"feed_alive": True, "screen": "overworld", "stuck_suspected": False, "note": "ok"}
        self.assertEqual(self.run_with(base), 0)
        self.assertEqual(self.run_with({**base, "screen": "home"}), 2)
        self.assertEqual(self.run_with({**base, "screen": "color_bars"}), 2)
        self.assertEqual(self.run_with({**base, "feed_alive": False}), 2)

    def test_request_shape(self):
        request = watchdog_shot.build_request(self.png)
        self.assertEqual(request["model"], "claude-haiku-5-5")
        self.assertEqual(request["output_config"]["effort"], "low")
        self.assertEqual(request["output_config"]["format"]["schema"]["properties"]["screen"]["enum"],
                         watchdog_shot.SCREENS)

    def test_api_error_exits_1(self):
        client = MagicMock()
        client.messages.create.side_effect = ConnectionError("down")
        with redirect_stdout(io.StringIO()):
            self.assertEqual(watchdog_shot.main([str(self.png)], client=client), 1)


if __name__ == "__main__":
    unittest.main()
