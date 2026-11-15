"""Offline tool tests: temporary profiles, fake bundles and injected signing only."""

import json
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile
import unittest

from lab_package import preflight, sha256, stage, verify
from lab_profiles import SETTINGS, read_json, seed, write_json


class ProfileTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "default"
        self.source.mkdir()
        (self.source / "UserSettings").mkdir()
        self.original = {
            "20-GlobalSettings": {
                "THEME": "dark", "CONSOLE_SETTINGS_COLLAPSED": False,
                "DEVICE_ALIASES": {"serial": "console-A"}, "STATS_FILE": "/live/stats",
                "COMMAND_LINE_TESTS": {"RUN": True}, "DEBUG": {"SelectedPanel": "MaxLair"},
                "DISCORD": {"secret": "do-not-copy"}, "FUTURE_SETTING": {"port": "live"},
            },
            "99-Panels": {"MaxLair": {"ConsoleSetup": {"Controller": "live-port"}}},
            "future": {"hardware": "do-not-copy"},
        }
        write_json(self.source / SETTINGS, self.original)

    def test_two_profiles_copy_preferences_without_live_state(self):
        before = (self.source / SETTINGS).read_bytes()
        for name in ("lab-A", "lab-B"):
            with self.subTest(profile=name):
                destination = seed(self.source, self.root, name)
                self.assertEqual(read_json(destination / SETTINGS), {
                    "20-GlobalSettings": {
                        "THEME": "dark", "CONSOLE_SETTINGS_COLLAPSED": False,
                        "LOG_WINDOW_STARTUP": False, "RICH_PRESENCE": False,
                    }, "99-Panels": {},
                })
                self.assertFalse(read_json(destination / "lab-profile.json")["hardware_configured"])
        self.assertEqual((self.source / SETTINGS).read_bytes(), before)

    def test_refuses_existing_profile_without_changing_it(self):
        destination = seed(self.source, self.root, "lab-A")
        original = (destination / SETTINGS).read_bytes()
        with self.assertRaisesRegex(ValueError, "already exists"):
            seed(self.source, self.root, "lab-A")
        self.assertEqual((destination / SETTINGS).read_bytes(), original)

    def test_invalid_or_default_names_are_refused(self):
        for name in ("", "default", "unit-tests", "../lab-A", "lab-", "lab-A/B", "lab-é"):
            with self.subTest(profile=name), self.assertRaises(ValueError):
                seed(self.source, self.root, name)

    def test_symlink_profile_directory_does_not_escape_root(self):
        outside = self.root / "outside"
        outside.mkdir()
        (self.root / "Profiles").symlink_to(outside, target_is_directory=True)
        with self.assertRaisesRegex(ValueError, "redirect"):
            seed(self.source, self.root, "lab-A")
        self.assertEqual(list(outside.iterdir()), [])


class PackageTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.app = self.root / "original.app"
        self.binary = self.app / "Contents/MacOS/SerialPrograms"
        self.binary.parent.mkdir(parents=True)
        self.binary.write_bytes(b"fake executable: must never be run")
        self.plist = self.app / "Contents/Info.plist"
        with self.plist.open("wb") as stream:
            plistlib.dump({"CFBundleExecutable": "SerialPrograms", "CFBundleIdentifier": "original"}, stream)
        self.receipt_path = Path(str(self.binary) + ".lab-build.json")
        self.receipt = {
            "schema": 1, "profile": "lab-A", "source_revision": "a" * 40,
            "source_clean": True, "binary_sha256": sha256(self.binary),
        }
        write_json(self.receipt_path, self.receipt)
        self.calls = []

    def signing(self, arguments):
        self.calls.append(arguments)
        if "--sign" in arguments:
            binary = Path(arguments[-1]) / "Contents/MacOS/SerialPrograms"
            binary.write_bytes(binary.read_bytes() + b":signed")

    def package(self, output="slot-A", peers=(), signer=None):
        return stage(self.app, self.root / output, "lab-A", "A",
                     "com.dayfine.pokemonautomation.labA", "test identity", peers,
                     signer or self.signing)

    def replace_receipt(self, receipt):
        self.receipt_path.write_text(json.dumps(receipt), encoding="utf-8")

    def test_round_trip_checks_post_signing_hash_and_preserves_original(self):
        original = self.binary.read_bytes(), self.plist.read_bytes()
        manifest = self.package()
        deployed = verify(manifest, "lab-A", signer=self.signing)
        self.assertEqual(deployed["source_revision"], "a" * 40)
        self.assertFalse(deployed["hardware_ownership_enforced"])
        self.assertNotEqual(deployed["binary_sha256"], self.receipt["binary_sha256"])
        self.assertEqual((self.binary.read_bytes(), self.plist.read_bytes()), original)
        self.assertEqual(len(self.calls), 3)

    def test_missing_receipt_is_refused_without_signing(self):
        self.receipt_path.unlink()
        with self.assertRaises(FileNotFoundError):
            self.package()
        self.assertEqual(self.calls, [])
        self.assertFalse((self.root / "slot-A").exists())

    def test_wrong_unbound_dirty_or_stale_receipt_is_refused(self):
        cases = [
            {"profile": ""}, {"profile": "lab-B"}, {"source_clean": False},
            {"source_revision": "short"}, {"binary_sha256": "0" * 64}, {"schema": 2},
        ]
        for changes in cases:
            with self.subTest(changes=changes), self.assertRaises(ValueError):
                self.replace_receipt(dict(self.receipt, **changes))
                self.package()
        self.assertEqual(self.calls, [])

    def test_binary_changed_after_build_is_refused(self):
        self.binary.write_bytes(b"a different build")
        with self.assertRaisesRegex(ValueError, "differs"):
            preflight(self.app, "lab-A")

    def test_peer_collision_on_each_identity_is_refused(self):
        peer = {"profile": "lab-B", "console": "B", "bundle_id": "com.dayfine.pokemonautomation.labB"}
        for field, value in (("profile", "lab-A"), ("console", "A"), ("bundle_id", "com.dayfine.pokemonautomation.labA")):
            with self.subTest(field=field), self.assertRaisesRegex(ValueError, field):
                path = self.root / (field + ".json")
                write_json(path, dict(peer, **{field: value}))
                self.package(peers=[path])
        self.assertEqual(self.calls, [])

    def test_existing_output_is_not_replaced(self):
        path = self.root / "slot-A"
        path.mkdir()
        (path / "keep").write_text("untouched")
        with self.assertRaises(FileExistsError):
            self.package()
        self.assertEqual((path / "keep").read_text(), "untouched")

    def test_sign_failure_removes_only_new_staging_directory(self):
        def failure(arguments):
            raise subprocess.CalledProcessError(1, "codesign")
        with self.assertRaises(subprocess.CalledProcessError):
            self.package(signer=failure)
        self.assertFalse((self.root / "slot-A").exists())
        self.assertTrue(self.binary.is_file())

    def test_package_change_or_wrong_profile_is_refused(self):
        manifest = self.package()
        with self.assertRaises(ValueError):
            verify(manifest, "lab-B", signer=self.signing)
        binary = manifest.parent / "SerialPrograms.app/Contents/MacOS/SerialPrograms"
        binary.write_bytes(b"changed after deployment")
        with self.assertRaisesRegex(ValueError, "changed"):
            verify(manifest, "lab-A", signer=self.signing)

    def test_signature_failure_is_not_ignored(self):
        manifest = self.package()
        def failure(arguments):
            raise subprocess.CalledProcessError(1, "codesign")
        with self.assertRaises(subprocess.CalledProcessError):
            verify(manifest, "lab-A", signer=failure)

    def test_relabeling_manifest_cannot_change_compiled_profile(self):
        manifest = self.package()
        data = read_json(manifest)
        data["profile"] = "lab-B"
        manifest.write_text(json.dumps(data), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "build receipt"):
            verify(manifest, "lab-B", signer=self.signing)

    def test_cli_rejects_old_or_unbound_build_without_launching_it(self):
        import sys
        self.receipt_path.unlink()
        tool = Path(__file__).with_name("lab_instance.py")
        result = subprocess.run([sys.executable, str(tool), "preflight", "--app", str(self.app),
                                 "--profile", "lab-A"], capture_output=True, text=True)
        self.assertEqual(result.returncode, 2)
        self.assertIn("refused", result.stderr)

    @unittest.skipUnless(shutil.which("cmake") and shutil.which("git"), "CMake and Git required")
    def test_actual_cmake_receipt_matches_binary_and_clears_unbound_receipt(self):
        source = self.root / "source"
        source.mkdir()
        def git(*arguments):
            return subprocess.run(["git", *arguments], cwd=source, check=True, capture_output=True)
        git("init")
        git("-c", "user.name=Test", "-c", "user.email=test@example.invalid", "commit", "--allow-empty", "-m", "fixture")
        script = Path(__file__).resolve().parents[2] / "SerialPrograms/cmake/LabBuildReceipt.cmake"
        def receipt(profile):
            subprocess.run(["cmake", "-DBINARY=" + str(self.binary), "-DPROFILE=" + profile,
                            "-DSOURCE=" + str(source), "-P", str(script)], check=True, capture_output=True)
        receipt("lab-A")
        self.assertEqual(preflight(self.app, "lab-A")["source_revision"], git("rev-parse", "HEAD").stdout.decode().strip())
        (source / "untracked").write_text("dirty")
        receipt("lab-A")
        with self.assertRaisesRegex(ValueError, "clean"):
            preflight(self.app, "lab-A")
        receipt("")
        self.assertFalse(self.receipt_path.exists())


if __name__ == "__main__":
    unittest.main()
