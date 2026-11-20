#!/usr/bin/env python3
"""Lab mGBA checkpoint runner and state library (Slice 4).

Reads local config (~/.config/pokemonautomation/mgba.json), loads a saved
checkpoint state into a running mGBA instance over MgbaLink v1, disconnects so
SerialPrograms can take the single bridge socket, runs SerialPrograms in
--headless --run mode, and optionally saves the resulting state on PASS.

The ROM path is read only from local config and never logged or passed to
SerialPrograms.
"""

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import mgba_link


DEFAULT_CONFIG_PATH = os.path.expanduser("~/.config/pokemonautomation/mgba.json")
DEFAULT_STATE_ROOT = os.path.expanduser("~/FRLGLab/mgba")
DEFAULT_LABEL = "fr-en"
DEFAULT_EXPECTED_CODE = "BPRE"
DEFAULT_PROGRAM_ID = "PokemonFRLG:AutoStory"


class RunnerError(Exception):
    """Raised when config, bridge, or state setup prevents a run."""


def load_config(config_path=DEFAULT_CONFIG_PATH):
    """Load local mGBA config JSON without exposing the ROM path in errors."""
    path = os.path.expanduser(config_path)
    try:
        with open(path, "r", encoding="utf-8") as f:
            raw = json.load(f)
    except OSError as exc:
        raise RunnerError(f"cannot read mGBA config {path}: {exc.strerror or exc}") from exc
    except json.JSONDecodeError as exc:
        raise RunnerError(f"invalid JSON in mGBA config {path}: {exc.msg}") from exc

    if not isinstance(raw, dict):
        raise RunnerError("mGBA config must be a JSON object")
    instances = raw.get("instances", {})
    if not isinstance(instances, dict) or not instances:
        raise RunnerError("mGBA config must define a non-empty 'instances' map")
    for name, port in instances.items():
        if not isinstance(port, int) or port <= 0 or port > 65535:
            raise RunnerError(f"invalid port for instance {name!r}")

    return {
        "rom": raw.get("rom", ""),
        "label": raw.get("label", DEFAULT_LABEL),
        "expected_code": raw.get("expected_code", DEFAULT_EXPECTED_CODE),
        "instances": dict(instances),
        "state_root": os.path.expanduser(raw.get("state_root", DEFAULT_STATE_ROOT)),
        "serial_programs": raw.get("serial_programs", ""),
    }


class StateLibrary:
    """Manages per-label checkpoint save states and per-instance .sav dirs."""

    def __init__(self, state_root, label=DEFAULT_LABEL):
        self.state_root = os.path.expanduser(state_root)
        self.label = label
        self.states_dir = os.path.join(self.state_root, label, "states")

    def state_path(self, checkpoint_id):
        return os.path.join(self.states_dir, f"{checkpoint_id}.ss")

    def has_state(self, checkpoint_id):
        return os.path.isfile(self.state_path(checkpoint_id))

    def list_states(self):
        if not os.path.isdir(self.states_dir):
            return []
        result = []
        for entry in sorted(os.listdir(self.states_dir)):
            if entry.endswith(".ss"):
                result.append(entry[:-3])
        return result

    def ensure_states_dir(self):
        os.makedirs(self.states_dir, exist_ok=True)
        return self.states_dir

    def ensure_instance_dir(self, instance_name, rom_path=""):
        """Create an isolated directory for an instance so .sav files never collide."""
        inst_dir = os.path.join(self.state_root, instance_name)
        os.makedirs(inst_dir, exist_ok=True)
        if rom_path and os.path.exists(rom_path):
            ext = os.path.splitext(rom_path)[1] or ".gba"
            link_path = os.path.join(inst_dir, f"rom{ext}")
            if not os.path.lexists(link_path):
                os.symlink(rom_path, link_path)
        return inst_dir


def _connect_and_verify(host, port, expected_code):
    link = mgba_link.MgbaLink(host, port)
    try:
        info = link.hello()
        if expected_code and info["code"] != expected_code:
            raise RunnerError(
                f"wrong ROM game code: got {info['code']!r}, expected {expected_code!r}"
            )
        return link, info
    except Exception:
        link.close()
        raise


def load_checkpoint_state(host, port, expected_code, state_path):
    """Connect to bridge, verify game code, load state, and disconnect."""
    if not os.path.isfile(state_path):
        raise RunnerError(f"missing checkpoint state: {state_path}")
    link, info = _connect_and_verify(host, port, expected_code)
    try:
        frame = link.load(state_path)
        return info["code"], frame
    finally:
        link.close()


def save_checkpoint_state(host, port, expected_code, state_path):
    """Connect to bridge, verify game code, save state, and disconnect."""
    os.makedirs(os.path.dirname(state_path), exist_ok=True)
    link, info = _connect_and_verify(host, port, expected_code)
    try:
        frame = link.save(state_path)
        return info["code"], frame
    finally:
        link.close()


def build_headless_command(binary, profile, program_id, options_path):
    return [
        binary,
        "--profile", profile,
        "--headless",
        "--run", program_id,
        "--options", options_path,
    ]


def run_checkpoint(
    config,
    instance_name,
    checkpoint_id,
    end_checkpoint=None,
    binary=None,
    program_id=DEFAULT_PROGRAM_ID,
    load_state=True,
    save_as=None,
    extra_options=None,
    command_builder=None,
):
    """Run one checkpoint from its saved state and return a verdict dict."""
    if instance_name not in config["instances"]:
        raise RunnerError(f"unknown instance {instance_name!r}")
    port = config["instances"][instance_name]
    host = mgba_link.DEFAULT_HOST
    expected_code = config["expected_code"]

    library = StateLibrary(config["state_root"], config["label"])
    library.ensure_instance_dir(instance_name, config.get("rom", ""))

    state_loaded = None
    game_code = None
    if load_state:
        state_path = library.state_path(checkpoint_id)
        game_code, _ = load_checkpoint_state(host, port, expected_code, state_path)
        state_loaded = state_path
    else:
        link, info = _connect_and_verify(host, port, expected_code)
        try:
            game_code = info["code"]
        finally:
            link.close()

    options_payload = {
        "START": checkpoint_id,
        "END": end_checkpoint or checkpoint_id,
    }
    if extra_options:
        options_payload.update(extra_options)

    exe = binary or config.get("serial_programs")
    if not exe and command_builder is None:
        raise RunnerError("SerialPrograms binary path is required (--binary or config.serial_programs)")

    profile = f"mgba-{instance_name}"
    start_time = time.monotonic()
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".json", prefix=f"mgba_{checkpoint_id}_", delete=False, encoding="utf-8"
    ) as tf:
        json.dump(options_payload, tf)
        options_path = tf.name

    try:
        cmd = (
            command_builder(profile, program_id, options_path)
            if command_builder is not None
            else build_headless_command(exe, profile, program_id, options_path)
        )
        proc = subprocess.run(cmd, capture_output=True, text=True)
    finally:
        if os.path.exists(options_path):
            os.remove(options_path)

    elapsed = round(time.monotonic() - start_time, 3)
    verdict = "PASS" if proc.returncode == 0 else "FAIL"

    state_saved = None
    if verdict == "PASS" and save_as:
        library.ensure_states_dir()
        save_path = library.state_path(save_as)
        save_checkpoint_state(host, port, expected_code, save_path)
        state_saved = save_path

    return {
        "checkpoint": checkpoint_id,
        "end": end_checkpoint or checkpoint_id,
        "instance": instance_name,
        "label": config["label"],
        "code": game_code,
        "verdict": verdict,
        "exit_code": proc.returncode,
        "duration_s": elapsed,
        "state_loaded": state_loaded,
        "state_saved": state_saved,
    }


def main(argv=None):
    parser = argparse.ArgumentParser(description="Run FRLG AutoStory checkpoints on mGBA.")
    parser.add_argument("--config", default=DEFAULT_CONFIG_PATH, help="Local mGBA JSON config path.")
    parser.add_argument("--instance", default="emu-1", help="Instance name from config.instances.")
    parser.add_argument("--checkpoint", required=True, help="Start checkpoint ID (e.g. 01.03).")
    parser.add_argument("--end", default=None, help="End checkpoint ID (defaults to --checkpoint).")
    parser.add_argument("--binary", default=None, help="Path to SerialPrograms executable.")
    parser.add_argument("--program", default=DEFAULT_PROGRAM_ID, help="Program identifier.")
    parser.add_argument("--no-load", action="store_true", help="Do not load <checkpoint>.ss before running.")
    parser.add_argument("--save-as", default=None, help="Save state as <id>.ss if the run passes.")
    parser.add_argument("--runs", type=int, default=1, help="Number of times to repeat the run.")
    args = parser.parse_args(argv)

    config = load_config(args.config)
    verdicts = []
    for i in range(args.runs):
        result = run_checkpoint(
            config=config,
            instance_name=args.instance,
            checkpoint_id=args.checkpoint,
            end_checkpoint=args.end,
            binary=args.binary,
            program_id=args.program,
            load_state=not args.no_load,
            save_as=args.save_as,
        )
        verdicts.append(result)
        print(json.dumps(result))
    return 0 if all(v["verdict"] == "PASS" for v in verdicts) else 1


if __name__ == "__main__":
    sys.exit(main())
