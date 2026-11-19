#!/usr/bin/env python3
"""Input-log replay and FrameReplay export for lab/mgba (Slice 5).

Loads a saved state into mGBA over MgbaLink v1, replays an input log
([{"frame": F, "mask": M}, ...]) under paced GRANT lockstep, records SHA-256
checksums of captured frames, and optionally exports a FrameReplay folder
(<ms>.png + input_log.json) compatible with Pokemon_FrameReplay.
"""

import argparse
import hashlib
import json
import os
import shutil
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mgba_link


def load_input_log(path):
    with open(path, "r", encoding="utf-8") as f:
        data = json.load(f)
    if not isinstance(data, list):
        raise ValueError("input log must be a JSON list")
    entries = []
    for item in data:
        entries.append({"frame": int(item["frame"]), "mask": int(item["mask"])})
    entries.sort(key=lambda e: e["frame"])
    return entries


def _sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            chunk = f.read(65536)
            if not chunk:
                break
            h.update(chunk)
    return h.hexdigest()


def replay_once(
    host,
    port,
    state_path,
    input_log,
    end_frame=None,
    expected_code=None,
    export_dir=None,
    sample_step=1,
):
    """Load `state_path`, replay `input_log` frame-by-frame, and return frame checksums."""
    if not os.path.isfile(state_path):
        raise FileNotFoundError(f"missing state file: {state_path}")
    if sample_step < 1:
        raise ValueError("sample_step must be >= 1")

    entries = sorted(
        ({"frame": int(e["frame"]), "mask": int(e["mask"])} for e in input_log),
        key=lambda e: e["frame"],
    )
    if export_dir:
        os.makedirs(export_dir, exist_ok=True)
        with open(os.path.join(export_dir, "input_log.json"), "w", encoding="utf-8") as f:
            json.dump(entries, f, indent=2)

    checksums = []
    with mgba_link.MgbaLink(host, port) as link:
        info = link.hello()
        if expected_code and info["code"] != expected_code:
            raise mgba_link.LinkError(
                f"wrong ROM game code: got {info['code']!r}, expected {expected_code!r}"
            )
        start_frame = link.load(state_path)
        target_end = end_frame
        if target_end is None:
            target_end = (entries[-1]["frame"] + 1) if entries else (start_frame + 1)

        idx = 0
        # Send any entries at or before start_frame before granting.
        cur = start_frame
        while cur <= target_end:
            while idx < len(entries) and entries[idx]["frame"] <= cur + 1:
                entry = entries[idx]
                late = link.keys(entry["frame"], entry["mask"])
                if late > 0:
                    raise mgba_link.LinkError(
                        f"KEYS for frame {entry['frame']} arrived {late} frame(s) late during replay"
                    )
                idx += 1

            if (cur - start_frame) % sample_step == 0:
                frame_info = link.frame()
                digest = _sha256_file(frame_info["path"])
                checksums.append({"frame": frame_info["frame"], "sha256": digest})
                if export_dir:
                    offset_ms = round((cur - start_frame) * 1000.0 / mgba_link.GBA_FPS)
                    dst = os.path.join(export_dir, f"{offset_ms}.png")
                    shutil.copyfile(frame_info["path"], dst)

            if cur == target_end:
                break
            cur += 1
            link.grant(cur)
            mgba_link._wait_for_frame(link, cur, mgba_link.FRAME_WAIT_TIMEOUT_SECONDS)

    return checksums


def verify_deterministic_replay(
    host,
    port,
    state_path,
    input_log,
    end_frame=None,
    expected_code=None,
    sample_step=1,
):
    """Replay the same input log twice from `state_path` and compare frame checksums."""
    run1 = replay_once(
        host=host,
        port=port,
        state_path=state_path,
        input_log=input_log,
        end_frame=end_frame,
        expected_code=expected_code,
        sample_step=sample_step,
    )
    run2 = replay_once(
        host=host,
        port=port,
        state_path=state_path,
        input_log=input_log,
        end_frame=end_frame,
        expected_code=expected_code,
        sample_step=sample_step,
    )
    return {
        "deterministic": run1 == run2,
        "frames_checked": len(run1),
        "run1": run1,
        "run2": run2,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Replay an mGBA input log from a save state.")
    parser.add_argument("--host", default=mgba_link.DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=mgba_link.DEFAULT_PORT)
    parser.add_argument("--state", required=True, help="Start save state (.ss).")
    parser.add_argument("--log", required=True, help="Input log JSON file.")
    parser.add_argument("--end-frame", type=int, default=None)
    parser.add_argument("--expected-code", default="BPRE")
    parser.add_argument("--export-dir", default=None, help="Optional FrameReplay output directory.")
    parser.add_argument("--verify", action="store_true", help="Replay twice and verify identical checksums.")
    args = parser.parse_args(argv)

    entries = load_input_log(args.log)
    if args.verify:
        result = verify_deterministic_replay(
            host=args.host,
            port=args.port,
            state_path=args.state,
            input_log=entries,
            end_frame=args.end_frame,
            expected_code=args.expected_code,
        )
        print(json.dumps({"deterministic": result["deterministic"], "frames_checked": result["frames_checked"]}))
        return 0 if result["deterministic"] else 1

    checksums = replay_once(
        host=args.host,
        port=args.port,
        state_path=args.state,
        input_log=entries,
        end_frame=args.end_frame,
        expected_code=args.expected_code,
        export_dir=args.export_dir,
    )
    print(json.dumps({"frames_checked": len(checksums), "checksums": checksums}))
    return 0


if __name__ == "__main__":
    sys.exit(main())
