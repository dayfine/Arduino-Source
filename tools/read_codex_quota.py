#!/usr/bin/env python3
"""Read the local Codex account's quota without starting a model turn."""

import datetime
import json
import os
import selectors
import subprocess
import sys
import time


def read_quota():
    proc = subprocess.Popen(
        ["codex", "app-server"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
    )
    selector = selectors.DefaultSelector()
    selector.register(proc.stdout, selectors.EVENT_READ)
    pending = b""

    def send(message):
        proc.stdin.write((json.dumps(message) + "\n").encode())
        proc.stdin.flush()

    def response(request_id):
        nonlocal pending
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            while b"\n" in pending:
                line, pending = pending.split(b"\n", 1)
                try:
                    message = json.loads(line)
                except ValueError:
                    continue
                if message.get("id") != request_id:
                    continue
                if "error" in message:
                    raise RuntimeError(
                        "Codex quota unavailable; app-server error code "
                        + str(message["error"].get("code"))
                    )
                return message["result"]
            for key, _ in selector.select(max(0, deadline - time.monotonic())):
                data = os.read(key.fileobj.fileno(), 65536)
                if not data:
                    raise RuntimeError("Codex app-server closed before replying")
                pending += data
        raise RuntimeError("Codex quota request timed out")

    try:
        send({
            "id": 1,
            "method": "initialize",
            "params": {
                "clientInfo": {"name": "quota_reader", "version": "0.1"},
                "capabilities": {"experimentalApi": True},
            },
        })
        response(1)
        send({"method": "initialized"})
        send({"id": 2, "method": "account/rateLimits/read"})
        result = response(2)
    finally:
        selector.close()
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
        proc.stdin.close()
        proc.stdout.close()

    buckets = result.get("rateLimitsByLimitId") or {}
    if not buckets and result.get("rateLimits"):
        bucket = result["rateLimits"]
        buckets = {bucket.get("limitId") or "default": bucket}
    windows = []
    for bucket_id, bucket in buckets.items():
        for name in ("primary", "secondary"):
            window = bucket.get(name)
            if not window:
                continue
            used = window.get("usedPercent")
            reset = window.get("resetsAt")
            windows.append({
                "bucket": bucket_id,
                "window": name,
                "duration_minutes": window.get("windowDurationMins"),
                "used_percent": used,
                "remaining_percent": None if used is None else 100 - used,
                "resets_at": None if reset is None else
                    datetime.datetime.fromtimestamp(reset).astimezone().isoformat(),
            })
    return {
        "source": "local Codex app-server account/rateLimits/read",
        "checked_at": datetime.datetime.now().astimezone().isoformat(),
        "windows": windows,
    }


if __name__ == "__main__":
    try:
        print(json.dumps(read_quota(), indent=2))
    except (OSError, RuntimeError, ValueError, KeyError) as error:
        print("Quota check failed: " + str(error), file=sys.stderr)
        sys.exit(1)
