#!/usr/bin/env python3
"""MgbaLink v1 smoke client for lab/mgba/bridge.lua (slice 0 of docs/design/Lab-mGBA-Console.md).

Talks to a running mGBA 0.10.5 that has bridge.lua loaded (step 0 in lab/mgba/README.md). It takes
only a host and a port; it never sees a ROM path.

    python3 -I lab/mgba/mgba_link.py hello
    python3 -I lab/mgba/mgba_link.py status
    python3 -I lab/mgba/mgba_link.py smoke [--stall-seconds 2] [--state-dir ~/FRLGLab/mgba/smoke]

Tests: python3 -I -m unittest discover -s lab/mgba -p test_mgba_link.py
"""

import argparse
import os
import socket
import statistics
import sys
import time

DEFAULT_HOST = "127.0.0.1"
DEFAULT_PORT = 47801
GBA_FPS = 59.7275

KEY = {
    "A": 1 << 0, "B": 1 << 1, "SELECT": 1 << 2, "START": 1 << 3, "RIGHT": 1 << 4,
    "LEFT": 1 << 5, "UP": 1 << 6, "DOWN": 1 << 7, "R": 1 << 8, "L": 1 << 9,
}


class LinkError(Exception):
    """The bridge replied ERR, closed the connection, or sent something unexpected."""


class LineReader:
    """Splits a byte stream into text lines (LF, optional CR before it)."""

    def __init__(self, recv, max_line=65536):
        self._recv = recv
        self._buf = b""
        self._max_line = max_line

    def read_line(self):
        while b"\n" not in self._buf:
            if len(self._buf) > self._max_line:
                raise LinkError("line longer than %d bytes" % self._max_line)
            chunk = self._recv(4096)
            if not chunk:
                raise LinkError("connection closed by the bridge")
            self._buf += chunk
        line, self._buf = self._buf.split(b"\n", 1)
        if line.endswith(b"\r"):
            line = line[:-1]
        return line.decode("utf-8", "replace")


def parse_hello(reply):
    """'OK 1 <code> <version> <mode>' -> dict."""
    parts = reply.split()
    if len(parts) != 5 or parts[0] != "OK" or parts[1] != "1":
        raise LinkError("bad HELLO reply: %r" % reply)
    return {"protocol": 1, "code": parts[2], "version": parts[3], "mode": parts[4]}


def parse_ok_frame(reply):
    """'OK <frame>' -> int."""
    parts = reply.split()
    if len(parts) != 2 or parts[0] != "OK" or not parts[1].isdigit():
        raise LinkError("expected 'OK <frame>', got %r" % reply)
    return int(parts[1])


def parse_keys(reply):
    """'OK' -> 0, 'LATE <n>' -> n."""
    parts = reply.split()
    if parts == ["OK"]:
        return 0
    if len(parts) == 2 and parts[0] == "LATE" and parts[1].isdigit():
        return int(parts[1])
    raise LinkError("bad KEYS reply: %r" % reply)


def parse_frame(reply):
    """'FRAME <frame> <w> <h> <path>' (0.10.5) -> dict. The path may contain spaces."""
    parts = reply.split(" ", 4)
    if len(parts) != 5 or parts[0] != "FRAME" or not all(p.isdigit() for p in parts[1:4]):
        raise LinkError("bad FRAME reply: %r" % reply)
    return {"frame": int(parts[1]), "w": int(parts[2]), "h": int(parts[3]), "path": parts[4]}


def parse_status(reply):
    """'OK key=value ...' -> dict; '-' becomes None, digits become int."""
    parts = reply.split()
    if not parts or parts[0] != "OK":
        raise LinkError("bad STATUS reply: %r" % reply)
    out = {}
    for item in parts[1:]:
        key, sep, value = item.partition("=")
        if not sep:
            raise LinkError("bad STATUS field %r" % item)
        if value == "-":
            out[key] = None
        elif value.isdigit():
            out[key] = int(value)
        else:
            out[key] = value
    return out


class MgbaLink:
    """One connection to one bridge. Every call sends one line and reads one reply line."""

    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=5.0):
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self._reader = LineReader(self._sock.recv)

    def close(self):
        """Says BYE (the bridge releases keys and grant at once), then closes."""
        try:
            self.command("BYE")
        except (LinkError, OSError):
            pass
        self._sock.close()

    def __enter__(self):
        return self

    def __exit__(self, *exc):
        self.close()

    def command(self, line):
        self._sock.sendall((line + "\n").encode("utf-8"))
        reply = self._reader.read_line()
        if reply.startswith("ERR"):
            raise LinkError("%s -> %s" % (line.split(" ", 1)[0], reply))
        return reply

    def hello(self):
        return parse_hello(self.command("HELLO 1"))

    def grant(self, frame):
        return parse_ok_frame(self.command("GRANT %d" % frame))

    def keys(self, frame, mask):
        return parse_keys(self.command("KEYS %d %d" % (frame, mask)))

    def frame(self):
        return parse_frame(self.command("FRAME"))

    def save(self, path):
        return parse_ok_frame(self.command("SAVE " + path))

    def load(self, path):
        return parse_ok_frame(self.command("LOAD " + path))

    def reset(self):
        self.command("RESET")

    def status(self):
        return parse_status(self.command("STATUS"))


def _wait_for_frame(link, target, timeout_s):
    """Polls STATUS until frame >= target; bounded by timeout_s."""
    deadline = time.monotonic() + timeout_s
    while True:
        st = link.status()
        if st["frame"] >= target:
            return st
        if time.monotonic() > deadline:
            raise LinkError("frame %d not reached in %.1f s (at %d)" % (target, timeout_s, st["frame"]))
        time.sleep(0.005)


def _ms_stats(samples):
    ms = sorted(s * 1000.0 for s in samples)
    return "min %.2f / median %.2f / max %.2f ms (n=%d)" % (ms[0], statistics.median(ms), ms[-1], len(ms))


def run_smoke(link, out, expect_code="BPRE", stall_seconds=2.0, state_path=None, frame_samples=30,
              key_mask=KEY["B"]):
    """Runs the slice-0 checks; writes one result line per check to out. Returns True if all pass.

    The game is held paused by a GRANT for the whole run and, when state_path is given, restored to
    the saved state at the end, so the key press leaves no trace. On disconnect the bridge releases
    the keys and the grant, and mGBA runs free again.
    """
    results = []

    def check(name, ok, detail):
        results.append(ok)
        out.write("- [%s] %s: %s\n" % ("x" if ok else " ", name, detail))
        out.flush()

    hello = link.hello()
    check("HELLO", hello["code"] == expect_code,
          "code %s, %s, mode %s" % (hello["code"], hello["version"], hello["mode"]))
    paced = hello["mode"] == "paced"

    # FRAME cost while running free.
    running = []
    for _ in range(frame_samples):
        t = time.perf_counter()
        fr = link.frame()
        running.append(time.perf_counter() - t)
    size = os.path.getsize(fr["path"]) if os.path.exists(fr["path"]) else 0
    check("FRAME round trip, running (includes the wait for the next frame)", size > 0 and (fr["w"], fr["h"]) == (240, 160),
          "%s; %dx%d, file %d bytes" % (_ms_stats(running), fr["w"], fr["h"], size))

    if not paced:
        check("GRANT stall", False, "bridge is in free mode; stall not tested")
        return all(results)

    # Hold the emulator where it is.
    cur = link.status()["frame"]
    link.grant(cur)
    time.sleep(0.1)
    f0 = link.status()["frame"]
    t0 = time.monotonic()
    time.sleep(stall_seconds)
    f1 = link.status()["frame"]
    held_s = time.monotonic() - t0
    check("GRANT stall", f1 == f0,
          "frame %d -> %d over %.2f s (check the mGBA window stayed responsive)" % (f0, f1, held_s))

    stalled = []
    for _ in range(frame_samples):
        t = time.perf_counter()
        link.frame()
        stalled.append(time.perf_counter() - t)
    check("FRAME cost, stalled (screenshot + reply)", True, _ms_stats(stalled))

    rtt = []
    for _ in range(frame_samples):
        t = time.perf_counter()
        link.status()
        rtt.append(time.perf_counter() - t)
    check("Command round trip, stalled", True, _ms_stats(rtt))

    saved = None
    if state_path:
        saved = link.save(state_path)
        check("SAVE", saved == f1, "OK %d at frame %d" % (saved, f1))

    # KEYS at frame F, read back from emu:getKeys() in the keysRead callback. The bridge applies a
    # mask at the first keysRead at or after F; mGBA fires keysRead only when the game reads the keys,
    # and in 0.10.5 the read for frame N comes before the frame callback that stalls at N. Grant
    # frame by frame (bounded) until the game has read the keys at or after F.
    f_key = f1 + 3
    late = link.keys(f_key, key_mask)
    target = f_key
    for _ in range(30):
        link.grant(target + 1)
        st = _wait_for_frame(link, target + 1, 5.0)
        if st["read_frame"] is not None and st["read_frame"] >= f_key:
            break
        target = st["frame"]
    check("KEYS at frame F (emu:getKeys)",
          late == 0 and st["applied_frame"] == f_key and st["applied_mask"] == key_mask
          and st["read_keys"] == key_mask,
          "F=%d: reply %s, applied at keysRead of frame %s with mask %s; getKeys at keysRead %s "
          "(frame %s); stalled at %d"
          % (f_key, "OK" if late == 0 else "LATE %d" % late, st["applied_frame"], st["applied_mask"],
             st["read_keys"], st["read_frame"], st["frame"]))
    link.keys(st["frame"], 0)
    link.grant(st["frame"] + 2)
    st = _wait_for_frame(link, st["frame"] + 2, 5.0)
    late_n = link.keys(st["frame"] - 2, 0)
    check("LATE", late_n >= 2, "KEYS 2 frames back -> LATE %d" % late_n)

    if state_path:
        loaded = link.load(state_path)
        after = link.status()["frame"]
        check("LOAD round trip", loaded == saved and after == saved,
              "SAVE %d, LOAD %d, frame after %d" % (saved, loaded, after))
    return all(results)


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--host", default=DEFAULT_HOST)
    ap.add_argument("--port", type=int, default=DEFAULT_PORT)
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("hello", help="print the HELLO reply")
    sub.add_parser("status", help="print the STATUS reply")
    sm = sub.add_parser("smoke", help="run the slice-0 acceptance checks; prints a Markdown checklist")
    sm.add_argument("--expect-code", default="BPRE")
    sm.add_argument("--stall-seconds", type=float, default=2.0)
    sm.add_argument("--samples", type=int, default=30)
    sm.add_argument("--state-dir", default=os.path.expanduser("~/FRLGLab/mgba/smoke"),
                    help="where the round-trip state goes (never committed)")
    args = ap.parse_args(argv)
    if args.cmd is None:
        ap.print_help()
        return 2
    with MgbaLink(args.host, args.port) as link:
        if args.cmd == "hello":
            print(link.command("HELLO 1"))
        elif args.cmd == "status":
            print(link.command("STATUS"))
        else:
            os.makedirs(args.state_dir, exist_ok=True)
            state = os.path.join(args.state_dir, "slice0.ss")
            ok = run_smoke(link, sys.stdout, args.expect_code, args.stall_seconds, state, args.samples)
            return 0 if ok else 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
