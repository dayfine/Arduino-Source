"""Tests for mgba_link.py: framing, reply parsing, and the smoke run against a fake bridge.

Run: python3 -I -m unittest discover -s lab/mgba -p test_mgba_link.py
No mGBA and no ROM: the fake bridge below models bridge.lua's frame clock in Python.
"""

import contextlib
import io
import os
import socket
import tempfile
import threading
import time
import unittest

import mgba_link


class FakeBridge:
    """A MgbaLink v1 server that advances one frame every tick unless a GRANT holds it."""

    def __init__(self, mode="paced", code="BPRE", frame_dir=None, delay_keys_reads=0):
        self.mode, self.code = mode, code
        self.frame, self.grant, self.held = 1000, None, None
        self.pending, self.states = [], {}
        self.applied = (None, None)
        self.read = (None, None)
        self.delay_keys_reads = delay_keys_reads
        self.skip_reads_range = None
        self.lock = threading.Lock()
        self.byes = 0
        self.frame_path = os.path.join(frame_dir, "frame.png")
        self.srv = socket.socket()
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(1)
        self.port = self.srv.getsockname()[1]
        self.stop = threading.Event()
        self.threads = [threading.Thread(target=self._serve, daemon=True),
                        threading.Thread(target=self._tick, daemon=True)]
        for t in self.threads:
            t.start()

    def close(self):
        self.stop.set()
        self.srv.close()
        for t in self.threads:
            t.join(2)

    def _tick(self):
        while not self.stop.is_set():
            with self.lock:
                if self.mode == "free" or self.grant is None or self.frame < self.grant:
                    if not (self.skip_reads_range and self.skip_reads_range[0] <= self.frame < self.skip_reads_range[1]):
                        self.pending.sort(key=lambda e: e[0])
                        while self.pending and self.pending[0][0] <= self.frame:
                            mask = self.pending.pop(0)[1]
                            if mask != self.held:
                                self.applied = (self.frame, mask)
                            self.held = mask
                        self.read = (self.frame, self.held or 0)
                    self.frame += 1
            time.sleep(0.001)

    def _serve(self):
        try:
            conn, _ = self.srv.accept()
        except OSError:
            return
        reader = mgba_link.LineReader(conn.recv)
        with conn:
            while not self.stop.is_set():
                try:
                    line = reader.read_line()
                except (mgba_link.LinkError, OSError):
                    return
                with self.lock:
                    reply = self._handle(line)
                conn.sendall((reply + "\n").encode())
                if reply == "OK BYE":
                    self.byes += 1
                    return

    def _handle(self, line):
        parts = line.split(" ", 1)
        cmd, rest = parts[0], (parts[1] if len(parts) > 1 else "")
        if cmd == "HELLO":
            return "OK 1 %s bridge-fake %s" % (self.code, self.mode)
        if cmd == "GRANT":
            if self.mode == "paced":
                self.grant = int(rest)
            return "OK %d" % self.frame
        if cmd == "KEYS":
            f, mask = (int(x) for x in rest.split())
            self.pending.append((f, mask))
            if f < self.frame:
                self.pending.remove((f, mask))
                self.held = mask
                self.applied = (self.frame, mask)
                return "LATE %d" % (self.frame - f)
            if self.delay_keys_reads > 0:
                self.skip_reads_range = (f, f + self.delay_keys_reads)
                self.delay_keys_reads = 0
            return "OK"
        if cmd == "FRAME":
            with open(self.frame_path, "wb") as fh:
                fh.write(b"\x89PNG fake")
            return "FRAME %d 240 160 %s" % (self.frame, self.frame_path)
        if cmd == "SAVE":
            self.states[rest] = self.frame
            return "OK %d" % self.frame
        if cmd == "LOAD":
            self.frame = self.states[rest]
            self.pending = []
            if self.grant is not None:
                self.grant = self.frame
            return "OK %d" % self.frame
        if cmd == "RESET":
            self.frame = 0
            self.pending = []
            if self.grant is not None:
                self.grant = self.frame
            return "OK"
        if cmd == "BYE":
            self.grant, self.held, self.pending = None, None, []
            return "OK BYE"
        if cmd == "STATUS":
            def o(v):
                return "-" if v is None else str(v)
            return ("OK frame=%d keys=%d read_frame=%s read_keys=%s applied_frame=%s applied_mask=%s grant=%s mode=%s"
                    % (self.frame, self.held or 0, o(self.read[0]), o(self.read[1]), o(self.applied[0]),
                       o(self.applied[1]), o(self.grant), self.mode))
        return "ERR unknown " + cmd


class LineReaderTest(unittest.TestCase):
    def reader(self, chunks):
        chunks = list(chunks)
        return mgba_link.LineReader(lambda n: chunks.pop(0) if chunks else b"")

    def test_split_across_chunks_and_crlf(self):
        r = self.reader([b"OK 1 BP", b"RE v paced\r\nOK", b" 5\nLATE 2\n"])
        self.assertEqual(r.read_line(), "OK 1 BPRE v paced")
        self.assertEqual(r.read_line(), "OK 5")
        self.assertEqual(r.read_line(), "LATE 2")

    def test_closed_connection(self):
        r = self.reader([b"OK"])
        with self.assertRaises(mgba_link.LinkError):
            r.read_line()

    def test_line_too_long(self):
        r = mgba_link.LineReader(lambda n: b"x" * 4096, max_line=10000)
        with self.assertRaises(mgba_link.LinkError):
            r.read_line()


class ParseTest(unittest.TestCase):
    def test_hello(self):
        self.assertEqual(mgba_link.parse_hello("OK 1 BPRE bridge-0.1 paced"),
                         {"protocol": 1, "code": "BPRE", "version": "bridge-0.1", "mode": "paced"})
        with self.assertRaises(mgba_link.LinkError):
            mgba_link.parse_hello("OK 2 BPRE x paced")

    def test_keys(self):
        self.assertEqual(mgba_link.parse_keys("OK"), 0)
        self.assertEqual(mgba_link.parse_keys("LATE 3"), 3)
        with self.assertRaises(mgba_link.LinkError):
            mgba_link.parse_keys("LATE")

    def test_frame_path_with_spaces(self):
        self.assertEqual(mgba_link.parse_frame("FRAME 12 240 160 /a b/c.png"),
                         {"frame": 12, "w": 240, "h": 160, "path": "/a b/c.png"})
        with self.assertRaises(mgba_link.LinkError):
            mgba_link.parse_frame("FRAME x 240 160 /p")

    def test_ok_frame_and_status(self):
        self.assertEqual(mgba_link.parse_ok_frame("OK 77"), 77)
        st = mgba_link.parse_status("OK frame=5 keys=2 grant=- mode=paced")
        self.assertEqual(st, {"frame": 5, "keys": 2, "grant": None, "mode": "paced"})
        with self.assertRaises(mgba_link.LinkError):
            mgba_link.parse_ok_frame("LATE 1")

    def test_key_bits_match_gba_keyinput(self):
        self.assertEqual(mgba_link.KEY["A"], 1)
        self.assertEqual(mgba_link.KEY["L"], 512)
        self.assertEqual(sum(mgba_link.KEY.values()), 0x3FF)


class SmokeAgainstFakeBridgeTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def run_smoke(self, **kw):
        bridge = FakeBridge(frame_dir=self.tmp.name, **kw)
        self.addCleanup(bridge.close)
        out = io.StringIO()
        with mgba_link.MgbaLink("127.0.0.1", bridge.port) as link:
            ok = mgba_link.run_smoke(link, out, stall_seconds=0.2,
                                     state_path=os.path.join(self.tmp.name, "s.ss"), frame_samples=3)
        return ok, out.getvalue()

    def test_all_checks_pass(self):
        ok, text = self.run_smoke()
        self.assertTrue(ok, text)
        for name in ("HELLO", "GRANT stall", "KEYS at frame F", "LATE", "SAVE", "LOAD round trip",
                     "FRAME cost, stalled"):
            self.assertIn("- [x] " + name, text)

    def test_wrong_game_code_fails(self):
        ok, text = self.run_smoke(code="BPEE")
        self.assertFalse(ok)
        self.assertIn("- [ ] HELLO", text)

    def test_free_mode_reports_untested_stall(self):
        ok, text = self.run_smoke(mode="free")
        self.assertFalse(ok)
        self.assertIn("- [ ] GRANT stall", text)

    def test_close_says_bye(self):
        bridge = FakeBridge(frame_dir=self.tmp.name)
        self.addCleanup(bridge.close)
        with mgba_link.MgbaLink("127.0.0.1", bridge.port) as link:
            link.grant(1)
        deadline = time.monotonic() + 2
        while bridge.byes == 0 and time.monotonic() < deadline:
            time.sleep(0.01)
        self.assertEqual(bridge.byes, 1)
        self.assertIsNone(bridge.grant)

    def test_err_reply_raises(self):
        bridge = FakeBridge(frame_dir=self.tmp.name)
        self.addCleanup(bridge.close)
        with mgba_link.MgbaLink("127.0.0.1", bridge.port) as link:
            with self.assertRaises(mgba_link.LinkError):
                link.command("BOGUS")

    def test_reset_clears_pending_and_resets_frame(self):
        bridge = FakeBridge(frame_dir=self.tmp.name)
        self.addCleanup(bridge.close)
        with mgba_link.MgbaLink("127.0.0.1", bridge.port) as link:
            link.grant(1000)
            link.keys(2000, mgba_link.KEY["A"])
            link.reset()
            st = link.status()
            self.assertEqual(st["frame"], 0)
            self.assertEqual(st["grant"], 0)
            self.assertEqual(bridge.pending, [])

    def test_keys_with_lag_passes(self):
        ok, text = self.run_smoke(delay_keys_reads=2)
        self.assertTrue(ok, text)
        self.assertIn("(lag 2 frames)", text)

    def test_samples_must_be_positive(self):
        with contextlib.redirect_stderr(io.StringIO()):
            with self.assertRaises(SystemExit) as ctx:
                mgba_link.main(["smoke", "--samples", "0"])
        self.assertEqual(ctx.exception.code, 2)


if __name__ == "__main__":
    unittest.main()
