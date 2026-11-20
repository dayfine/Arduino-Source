"""Tests for lab/mgba/replay.py: deterministic input-log replay and FrameReplay export."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import replay
import test_mgba_link


class ReplayFakeBridge(test_mgba_link.FakeBridge):
    """Multi-accept FakeBridge that writes deterministic frame bytes per (frame, mask)."""

    def __init__(self, frame_dir, diverge_on_load=None):
        super().__init__(mode="paced", code="BPRE", frame_dir=frame_dir)
        self.load_count = 0
        self.diverge_on_load = diverge_on_load

    def _serve(self):
        while not self.stop.is_set():
            try:
                conn, _ = self.srv.accept()
            except OSError:
                return
            reader = test_mgba_link.mgba_link.LineReader(conn.recv)
            with conn:
                while not self.stop.is_set():
                    try:
                        line = reader.read_line()
                    except (test_mgba_link.mgba_link.LinkError, OSError):
                        break
                    with self.lock:
                        reply = self._handle(line)
                    conn.sendall((reply + "\n").encode())
                    if reply == "OK BYE":
                        self.byes += 1
                        break

    def _handle(self, line):
        if line.startswith("LOAD "):
            self.load_count += 1
            self.held = 0
            self.pending.clear()
        if line == "FRAME":
            payload = f"frame={self.frame};mask={self.held or 0}".encode("utf-8")
            if self.diverge_on_load == self.load_count:
                payload += b";diverged"
            with open(self.frame_path, "wb") as f:
                f.write(payload)
            return f"FRAME {self.frame} 240 160 {self.frame_path}"
        return super()._handle(line)


class ReplayTests(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.state_path = os.path.join(self.tmpdir.name, "01.03.ss")
        with open(self.state_path, "wb") as f:
            f.write(b"state")

    def tearDown(self):
        self.tmpdir.cleanup()

    def test_deterministic_replay_twice_gives_identical_checksums(self):
        bridge = ReplayFakeBridge(frame_dir=self.tmpdir.name)
        bridge.states[self.state_path] = 1000
        try:
            log = [
                {"frame": 1001, "mask": 1},
                {"frame": 1003, "mask": 0},
            ]
            res = replay.verify_deterministic_replay(
                host="127.0.0.1",
                port=bridge.port,
                state_path=self.state_path,
                input_log=log,
                end_frame=1004,
                expected_code="BPRE",
            )
            self.assertTrue(res["deterministic"])
            self.assertEqual(res["frames_checked"], 5)
            self.assertEqual(res["run1"], res["run2"])
        finally:
            bridge.close()

    def test_divergent_replay_is_detected(self):
        bridge = ReplayFakeBridge(frame_dir=self.tmpdir.name, diverge_on_load=2)
        bridge.states[self.state_path] = 1000
        try:
            log = [{"frame": 1001, "mask": 1}]
            res = replay.verify_deterministic_replay(
                host="127.0.0.1",
                port=bridge.port,
                state_path=self.state_path,
                input_log=log,
                end_frame=1002,
                expected_code="BPRE",
            )
            self.assertFalse(res["deterministic"])
        finally:
            bridge.close()

    def test_replay_exports_frame_replay_folder(self):
        bridge = ReplayFakeBridge(frame_dir=self.tmpdir.name)
        bridge.states[self.state_path] = 1000
        export_dir = os.path.join(self.tmpdir.name, "exported_case")
        try:
            log = [{"frame": 1001, "mask": 2}]
            checksums = replay.replay_once(
                host="127.0.0.1",
                port=bridge.port,
                state_path=self.state_path,
                input_log=log,
                end_frame=1002,
                expected_code="BPRE",
                export_dir=export_dir,
            )
            self.assertEqual(len(checksums), 3)
            self.assertTrue(os.path.isfile(os.path.join(export_dir, "0.png")))
            self.assertTrue(os.path.isfile(os.path.join(export_dir, "input_log.json")))
            with open(os.path.join(export_dir, "input_log.json"), "r", encoding="utf-8") as f:
                saved_log = json.load(f)
            self.assertEqual(saved_log, log)
        finally:
            bridge.close()


if __name__ == "__main__":
    unittest.main()
