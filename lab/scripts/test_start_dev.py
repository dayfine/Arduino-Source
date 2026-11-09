"""Offline launcher tests: python3 -m unittest discover -s lab/scripts -p test_start_dev.py -v."""

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import patch

import start_dev as dev


class ConfigurationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.slot = dict(worktree=str(self.root / 'src-A'), branch='lab/dev-A',
                         build_dir=str(self.root / 'build-A'), stage_root=str(self.root / 'stage-A'),
                         bundle_id='com.dayfine.pokemonautomation.labA', signing_identity='Local Development')
        self.data = {'schema': 1, 'slots': {'A': self.slot}}
        self.path = self.root / 'config.json'

    def load(self):
        self.path.write_text(json.dumps(self.data))
        return dev.load_config(self.path, 'A')

    def test_profile_and_console_are_bound_by_slot(self):
        slot = self.load()
        self.assertEqual(('A', 'lab-A'), (slot['console'], slot['profile']))

    def test_refuses_feature_branch_default_identity_and_ad_hoc_signing(self):
        for key, value in [('branch', 'swsh/feature'), ('branch', 'lab/../main'),
                           ('bundle_id', 'PokemonAutomation.SerialPrograms'), ('signing_identity', '-')]:
            with self.subTest(key=key, value=value):
                self.data['slots']['A'] = dict(self.slot, **{key: value})
                with self.assertRaises(ValueError):
                    self.load()

    def test_refuses_nested_shared_or_relative_directories(self):
        for value in [self.slot['worktree'], str(Path(self.slot['worktree']) / 'build'), 'relative']:
            with self.subTest(value=value):
                self.data['slots']['A'] = dict(self.slot, build_dir=value)
                with self.assertRaises(ValueError):
                    self.load()

    def test_two_slots_must_have_distinct_paths_and_identities(self):
        second = dict(self.slot, worktree=str(self.root / 'src-B'), build_dir=str(self.root / 'build-B'),
                      stage_root=str(self.root / 'stage-B'), bundle_id='com.dayfine.pokemonautomation.labB')
        self.data['slots']['B'] = second
        self.load()
        for key in ['worktree', 'build_dir', 'stage_root', 'bundle_id']:
            with self.subTest(key=key):
                self.data['slots']['B'] = dict(second, **{key: self.slot[key]})
                with self.assertRaises(ValueError):
                    self.load()

    def test_build_settings_cannot_override_profile_or_command_mode(self):
        for args in [['-DPA_LAB_PROFILE=default'], ['-P', '/tmp/script.cmake'],
                     ['--preset=release'], ['-DCMAKE_PROJECT_INCLUDE=/tmp/override.cmake']]:
            with self.subTest(args=args):
                self.data['slots']['A'] = dict(self.slot, cmake_args=args)
                with self.assertRaises(ValueError):
                    self.load()
        self.data['slots']['A'] = dict(self.slot, cmake_args=['-DCMAKE_BUILD_TYPE=RelWithDebInfo'])
        self.load()


class OwnershipTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)

    def test_same_console_refused_different_console_allowed(self):
        with dev.console_lock('A', self.root) as token:
            with self.assertRaises(ValueError):
                with dev.console_lock('A', self.root):
                    self.fail('second owner admitted')
            with dev.console_lock('B', self.root):
                self.assertEqual(token, (self.root / 'locks/A/token').read_text())
        self.assertFalse((self.root / 'locks/A').exists())

    def test_exception_releases_own_lock(self):
        with self.assertRaises(RuntimeError):
            with dev.console_lock('A', self.root):
                raise RuntimeError('build failed')
        self.assertFalse((self.root / 'locks/A').exists())

    def test_older_format_lock_folder_refused_and_kept(self):
        (self.root / 'locks/A').mkdir(parents=True)
        with self.assertRaises(ValueError):
            with dev.console_lock('A', self.root):
                self.fail('second owner admitted')
        self.assertTrue((self.root / 'locks/A').is_dir())
        self.assertEqual(['A'], [p.name for p in (self.root / 'locks').iterdir()])

    def test_existing_shell_helpers_see_the_same_ownership(self):
        helper = Path(__file__).resolve().parents[1] / 'control/lablock.sh'
        with dev.console_lock('A', self.root) as token:
            env = dict(os.environ, LAB_DIR=str(self.root))
            env.pop('LABLOCK_TOKEN', None)
            refused = subprocess.run(['sh', str(helper), '-c', 'A', 'check'], env=env, capture_output=True)
            self.assertEqual(3, refused.returncode)
            allowed = subprocess.run(['sh', str(helper), '-c', 'A', 'check'],
                                     env=dict(env, LABLOCK_TOKEN=token), capture_output=True)
            self.assertEqual(0, allowed.returncode)

    def test_build_is_exclusive_even_between_different_consoles(self):
        with dev.build_lock(self.root):
            with self.assertRaises(ValueError):
                with dev.build_lock(self.root):
                    self.fail('parallel build admitted')
        with dev.build_lock(self.root):
            pass

    def test_interrupt_keeps_console_lock_through_child_teardown(self):
        # The "app" is a temporary Python process; no GUI, settings or hardware.
        child = self.root / 'child.py'
        ready, exited = self.root / 'ready', self.root / 'exited'
        child.write_text('''import signal, time
from pathlib import Path
import sys
def stop(*args):
    assert Path(sys.argv[1]).is_dir(), 'ownership released before child exit'
    time.sleep(0.2)
    assert Path(sys.argv[1]).is_dir(), 'ownership released during teardown'
    Path(sys.argv[3]).write_text('clean exit')
    sys.exit(0)
signal.signal(signal.SIGTERM, stop)
Path(sys.argv[2]).touch()
while True: time.sleep(0.05)
''')
        supervisor = self.root / 'supervisor.py'
        supervisor.write_text('''import sys
from pathlib import Path
sys.path.insert(0, sys.argv[1])
import start_dev as dev
try:
    with dev.console_lock('A', sys.argv[2]):
        dev.run([sys.executable, sys.argv[3], sys.argv[4], sys.argv[5], sys.argv[6]])
except KeyboardInterrupt:
    sys.exit(130)
''')
        proc = subprocess.Popen([sys.executable, str(supervisor), str(Path(dev.__file__).parent),
                                 str(self.root), str(child), str(self.root / 'locks/A'), str(ready), str(exited)],
                                stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        try:
            deadline = time.monotonic() + 5
            while not ready.exists() and proc.poll() is None and time.monotonic() < deadline:
                time.sleep(0.01)
            self.assertTrue(ready.exists(), 'temporary child did not start')
            proc.send_signal(signal.SIGTERM)
            output, error = proc.communicate(timeout=5)
            self.assertEqual(130, proc.returncode, output + error)
            self.assertEqual('clean exit', exited.read_text())
            self.assertFalse((self.root / 'locks/A').exists())
        finally:
            if proc.poll() is None:
                proc.kill()
                proc.communicate()


class PipelineTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.slot = dict(console='A', profile='lab-A', worktree=str(self.root / 'src'), branch='lab/dev-A',
                         build_dir=str(self.root / 'build'), stage_root=str(self.root / 'stage'),
                         bundle_id='com.dayfine.pokemonautomation.labA', signing_identity='Local Development')
        self.revision = 'a' * 40
        receipt = Path(self.slot['build_dir']) / 'SerialPrograms.app/Contents/MacOS/SerialPrograms.lab-build.json'
        receipt.parent.mkdir(parents=True)
        receipt.write_text(json.dumps({'source_revision': self.revision}))
        self.calls = []

    def runner(self, command, **kwargs):
        self.calls.append((command, kwargs))
        if command[0] == 'git':
            return self.revision if command[-2:] == ['rev-parse', 'HEAD'] else ''
        if command[0].endswith('/SerialPrograms'):
            lock = self.root / 'locks/A'
            self.assertTrue(lock.is_dir(), 'lock must outlive actual GUI')
            self.assertEqual((lock / 'token').read_text(), kwargs['env']['LABLOCK_TOKEN'])
            # Build lock has already been released: the other console may now build.
            with dev.build_lock(self.root):
                pass

    def execute(self, runner=None):
        with patch.object(dev, 'prepare_source', return_value=self.revision), \
             patch.object(dev, 'validate_profile', return_value=self.root / 'profile'), \
             patch.object(dev, 'copy_runtime_data'), \
             redirect_stdout(io.StringIO()):
            dev.start(self.slot, self.root, runner or self.runner)

    def test_sequence_binds_build_then_signs_verifies_and_runs_direct_child(self):
        self.execute()
        commands = [command for command, _ in self.calls]
        self.assertIn('-DPA_LAB_PROFILE=lab-A', commands[0])
        self.assertEqual(['cmake', '--build', self.slot['build_dir'], '--parallel', '4'], commands[1])
        self.assertEqual('stage', commands[4][2])
        self.assertEqual('verify', commands[5][2])
        self.assertEqual(['--profile', 'lab-A'], commands[-1][-2:])
        self.assertFalse((self.root / 'locks/A').exists())

    def test_each_build_stage_or_verify_failure_prevents_launch(self):
        for failure in ['build', 'stage', 'verify']:
            with self.subTest(failure=failure):
                self.calls.clear()
                def failing(command, **kwargs):
                    if ('--build' in command if failure == 'build' else failure in command):
                        raise subprocess.CalledProcessError(1, command)
                    return self.runner(command, **kwargs)
                with self.assertRaises(subprocess.CalledProcessError):
                    self.execute(failing)
                self.assertFalse(any(cmd[0].endswith('/SerialPrograms') for cmd, _ in self.calls))
                self.assertFalse((self.root / 'locks/A').exists())

    def test_changed_revision_after_build_prevents_staging(self):
        def changed(command, **kwargs):
            result = self.runner(command, **kwargs)
            return 'b' * 40 if command[-2:] == ['rev-parse', 'HEAD'] else result
        with self.assertRaisesRegex(ValueError, 'revision changed'):
            self.execute(changed)
        self.assertFalse(any('stage' in cmd for cmd, _ in self.calls))

    def test_runtime_directories_are_copied_beside_app_and_missing_data_is_refused(self):
        build, stage = self.root / 'data-build', self.root / 'data-stage'
        for name in ['Resources', 'Firmware', 'Scripts']:
            (build / name).mkdir(parents=True)
            (build / name / 'fixture.txt').write_text(name)
        stage.mkdir()
        dev.copy_runtime_data(build, stage)
        for name in ['Resources', 'Firmware', 'Scripts']:
            self.assertEqual(name, (stage / name / 'fixture.txt').read_text())
        with self.assertRaisesRegex(ValueError, 'missing runtime directory'):
            dev.copy_runtime_data(self.root / 'missing', self.root / 'unused')

    def test_cache_cannot_cross_source_or_profile(self):
        cache = Path(self.slot['build_dir']) / 'CMakeCache.txt'
        for source, profile in [('/other/source', 'lab-A'), (self.slot['worktree'] + '/SerialPrograms', 'lab-B')]:
            with self.subTest(source=source, profile=profile):
                cache.write_text(f'CMAKE_HOME_DIRECTORY:INTERNAL={source}\nPA_LAB_PROFILE:STRING={profile}\n')
                with self.assertRaises(ValueError):
                    dev.check_build_cache(self.slot)


class SourceTests(unittest.TestCase):
    def test_dirty_worktree_stops_before_fetch_or_switch(self):
        calls = []
        slot = {'worktree': '/temporary/source', 'branch': 'lab/dev-A'}
        def runner(command, **kwargs):
            calls.append(command)
            return slot['worktree'] if command[-1] == '--show-toplevel' else ' M changed.cpp'
        with self.assertRaisesRegex(ValueError, 'dirty'):
            dev.prepare_source(slot, runner)
        self.assertEqual(2, len(calls))


class GitIntegrationTests(unittest.TestCase):
    def test_real_checkout_switch_and_fast_forward_preserve_other_branch(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            remote, checkout = root / 'origin', root / 'worktree'
            def git(path, *args):
                return subprocess.run(['git', '-C', str(path), *args], check=True,
                                      text=True, capture_output=True).stdout.strip()
            subprocess.run(['git', 'init', '-b', 'lab/main', str(remote)], check=True, capture_output=True)
            git(remote, 'config', 'user.email', 'fixture@example.invalid')
            git(remote, 'config', 'user.name', 'Launcher test')
            tool = remote / 'devtools/lab/lab_instance.py'
            tool.parent.mkdir(parents=True)
            tool.write_text('# temporary offline fixture\n')
            git(remote, 'add', '.')
            git(remote, 'commit', '-m', 'Fixture guard base')
            guard = git(remote, 'rev-parse', 'HEAD')
            git(remote, 'switch', '-c', 'lab/dev-A')
            subprocess.run(['git', 'clone', '--branch', 'lab/main', str(remote), str(checkout)],
                           check=True, capture_output=True)
            slot = {'worktree': str(checkout), 'branch': 'lab/dev-A'}
            with patch.object(dev, 'DEVICE_GUARD_REVISION', guard), redirect_stdout(io.StringIO()):
                self.assertEqual(guard, dev.prepare_source(slot))
                self.assertEqual('lab/dev-A', git(checkout, 'branch', '--show-current'))
                (remote / 'new.txt').write_text('new remote commit')
                git(remote, 'add', '.')
                git(remote, 'commit', '-m', 'Advance selected lab branch')
                newest = git(remote, 'rev-parse', 'HEAD')
                self.assertEqual(newest, dev.prepare_source(slot))
                self.assertEqual(guard, git(checkout, 'rev-parse', 'lab/main'))
                git(checkout, 'switch', '-c', 'lab/local-only')
                local_slot = dict(slot, branch='lab/local-only')
                self.assertEqual(newest, dev.prepare_source(local_slot))
                git(checkout, 'switch', slot['branch'])
                # A local untracked file must survive, and stop all switching/updating.
                (checkout / 'user.txt').write_text('keep this')
                with self.assertRaisesRegex(ValueError, 'dirty'):
                    dev.prepare_source(slot)
                self.assertEqual('keep this', (checkout / 'user.txt').read_text())


if __name__ == '__main__':
    unittest.main()
