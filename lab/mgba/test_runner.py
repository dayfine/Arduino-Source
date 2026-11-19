"""Tests for lab/mgba/runner.py: config privacy, state library, and repeated checkpoint runs."""

import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import runner
import test_mgba_link


class MultiAcceptFakeBridge(test_mgba_link.FakeBridge):
    """FakeBridge that accepts sequential connections (LOAD, then app, then SAVE)."""

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


class RunnerTests(unittest.TestCase):
    def setUp(self):
        self.tmpdir = tempfile.TemporaryDirectory()
        self.bridge = MultiAcceptFakeBridge(mode="paced", code="BPRE", frame_dir=self.tmpdir.name)
        self.state_root = os.path.join(self.tmpdir.name, "states_root")
        self.rom_file = os.path.join(self.tmpdir.name, "secret_local_rom_name.gba")
        with open(self.rom_file, "wb") as f:
            f.write(b"fake")
        self.config_path = os.path.join(self.tmpdir.name, "mgba.json")
        with open(self.config_path, "w", encoding="utf-8") as f:
            json.dump(
                {
                    "rom": self.rom_file,
                    "label": "fr-en",
                    "expected_code": "BPRE",
                    "instances": {"emu-1": self.bridge.port},
                    "state_root": self.state_root,
                },
                f,
            )
        self.lib = runner.StateLibrary(self.state_root, "fr-en")
        self.lib.ensure_states_dir()
        with open(self.lib.state_path("01.03"), "wb") as f:
            f.write(b"state-01.03")
        self.bridge.states[self.lib.state_path("01.03")] = 1000

    def tearDown(self):
        self.bridge.close()
        self.tmpdir.cleanup()

    def test_config_and_rom_privacy(self):
        cfg = runner.load_config(self.config_path)
        self.assertEqual(cfg["label"], "fr-en")
        self.assertEqual(cfg["instances"]["emu-1"], self.bridge.port)

        res = runner.run_checkpoint(
            config=cfg,
            instance_name="emu-1",
            checkpoint_id="01.03",
            command_builder=lambda prof, prog, opts: [sys.executable, "-c", "import sys; sys.exit(0)"],
        )
        serialized = json.dumps(res)
        self.assertNotIn("secret_local_rom_name", serialized)
        self.assertEqual(res["code"], "BPRE")

    def test_wrong_game_code_refused(self):
        cfg = runner.load_config(self.config_path)
        cfg["expected_code"] = "BPGE"
        with self.assertRaises(runner.RunnerError) as ctx:
            runner.run_checkpoint(
                config=cfg,
                instance_name="emu-1",
                checkpoint_id="01.03",
                command_builder=lambda prof, prog, opts: [sys.executable, "-c", "import sys; sys.exit(0)"],
            )
        self.assertIn("BPRE", str(ctx.exception))
        self.assertNotIn("secret_local_rom_name", str(ctx.exception))

    def test_run_twice_from_state_with_verdicts(self):
        cfg = runner.load_config(self.config_path)
        seen_cmds = []

        def build_cmd(profile, program_id, options_path):
            with open(options_path, "r", encoding="utf-8") as f:
                opts = json.load(f)
            seen_cmds.append((profile, program_id, opts))
            return [sys.executable, "-c", "import sys; sys.exit(0)"]

        v1 = runner.run_checkpoint(cfg, "emu-1", "01.03", command_builder=build_cmd)
        v2 = runner.run_checkpoint(cfg, "emu-1", "01.03", command_builder=build_cmd)
        self.assertEqual(v1["verdict"], "PASS")
        self.assertEqual(v2["verdict"], "PASS")
        self.assertEqual(len(seen_cmds), 2)
        self.assertEqual(seen_cmds[0][0], "mgba-emu-1")
        self.assertEqual(seen_cmds[0][1], "PokemonFRLG:AutoStory")
        self.assertEqual(seen_cmds[0][2]["START"], "01.03")
        self.assertTrue(os.path.islink(os.path.join(self.state_root, "emu-1", "rom.gba")))

    def test_failed_run_does_not_save_state(self):
        cfg = runner.load_config(self.config_path)
        res = runner.run_checkpoint(
            cfg,
            "emu-1",
            "01.03",
            save_as="01.04",
            command_builder=lambda prof, prog, opts: [sys.executable, "-c", "import sys; sys.exit(1)"],
        )
        self.assertEqual(res["verdict"], "FAIL")
        self.assertEqual(res["exit_code"], 1)
        self.assertIsNone(res["state_saved"])
        self.assertFalse(self.lib.has_state("01.04"))

    def test_passing_run_saves_next_state(self):
        cfg = runner.load_config(self.config_path)
        res = runner.run_checkpoint(
            cfg,
            "emu-1",
            "01.03",
            save_as="01.04",
            command_builder=lambda prof, prog, opts: [sys.executable, "-c", "import sys; sys.exit(0)"],
        )
        self.assertEqual(res["verdict"], "PASS")
        self.assertEqual(res["state_saved"], self.lib.state_path("01.04"))
        self.assertIn(self.lib.state_path("01.04"), self.bridge.states)


if __name__ == "__main__":
    unittest.main()
