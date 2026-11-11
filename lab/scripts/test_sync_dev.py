"""Offline Git integration tests: python3 -m unittest discover -s lab/scripts -p test_sync_dev.py -v."""

import json
from pathlib import Path
import subprocess
import tempfile
import unittest

import sync_dev


class FeatureSyncTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.git('init', '-q', '-b', 'lab/dev-test')
        self.git('config', 'user.name', 'Test')
        self.git('config', 'user.email', 'test@example.invalid')
        self.git('config', 'commit.gpgSign', 'false')
        self.git('config', 'core.hooksPath', '/dev/null')
        self.file = self.root / 'feature.txt'
        self.guard = self.root / 'lab-guard.txt'
        (self.root / 'SerialPrograms').mkdir()
        (self.root / 'SerialPrograms/CMakeLists.txt').write_text('# test code branch\n')
        self.file.write_text('base\n')
        self.guard.write_text('base\n')
        self.commit('base')
        self.git('branch', 'feature/demo')
        self.guard.write_text('lab protection\n')
        self.commit('lab safeguards')
        self.git('switch', '-q', 'feature/demo')
        self.file.write_text('version one\nstale line\n')
        self.commit('feature v1')
        self.old = self.rev('HEAD')
        self.git('switch', '-q', 'lab/dev-test')
        self.git('merge', '--no-edit', 'feature/demo')
        self.integration = self.rev('HEAD')

    def git(self, *args, check=True):
        result = subprocess.run(['git', '-C', str(self.root), *args], capture_output=True)
        if check and result.returncode:
            self.fail(result.stderr.decode())
        return result.stdout.decode().strip()

    def rev(self, ref):
        return self.git('rev-parse', ref)

    def commit(self, message):
        self.git('add', '.')
        self.git('commit', '-qm', message)

    def amend(self):
        self.git('switch', '-q', 'feature/demo')
        self.file.write_text('version two\n')
        self.git('add', '.')
        self.git('commit', '--amend', '-qm', 'feature v2')
        self.new = self.rev('HEAD')
        self.git('switch', '-q', 'lab/dev-test')

    def test_amend_replaces_old_feature_and_keeps_lab_safeguards(self):
        self.amend()
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual('version two\n', self.file.read_text())
        self.assertEqual('lab protection\n', self.guard.read_text())
        self.assertEqual([self.integration, self.new], self.git('show', '-s', '--format=%P', 'HEAD').split())
        self.assertEqual('', self.git('status', '--porcelain'))

    def test_existing_conflict_is_backed_up_then_replaced(self):
        self.amend()
        self.git('merge', '--no-edit', 'feature/demo', check=False)
        conflict = self.file.read_bytes()
        self.assertIn(b'<<<<<<<', conflict)
        backup = sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertIsNotNone(backup)
        self.assertEqual(conflict, (backup / 'files/feature.txt').read_bytes())
        self.assertTrue((backup / 'index').is_file())
        self.assertEqual(self.new, json.loads((backup / 'manifest.json').read_text())['incoming'])
        self.assertEqual('version two\n', self.file.read_text())
        self.assertEqual('', self.git('status', '--porcelain'))

    def test_force_refuses_integration_only_change_in_same_file(self):
        self.file.write_text('integration modification\n')
        self.commit('integration customization')
        self.amend()
        head = self.rev('HEAD')
        with self.assertRaisesRegex(ValueError, 'integration changes'):
            sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual(head, self.rev('HEAD'))
        self.assertEqual('integration modification\n', self.file.read_text())

    def test_normal_commit_after_amend_merges_normally(self):
        self.amend()
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.git('switch', '-q', 'feature/demo')
        self.file.write_text('version three\n')
        self.commit('feature v3')
        self.git('switch', '-q', 'lab/dev-test')
        sync_dev.sync_feature(self.root, 'feature/demo')
        self.assertEqual('version three\n', self.file.read_text())
        self.assertEqual('lab protection\n', self.guard.read_text())

    def test_repeat_amend_and_noop(self):
        self.amend()
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        first = self.rev('HEAD')
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual(first, self.rev('HEAD'))
        self.git('switch', '-q', 'feature/demo')
        self.file.write_text('version four\n')
        self.git('add', '.')
        self.git('commit', '--amend', '-qm', 'feature v4')
        self.git('switch', '-q', 'lab/dev-test')
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual('version four\n', self.file.read_text())

    def test_dirty_unrelated_work_is_not_discarded(self):
        self.amend()
        self.guard.write_text('uncommitted work\n')
        head = self.rev('HEAD')
        with self.assertRaisesRegex(ValueError, 'dirty'):
            sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual(head, self.rev('HEAD'))
        self.assertEqual('uncommitted work\n', self.guard.read_text())

    def test_other_pending_merge_is_not_aborted(self):
        self.amend()
        self.git('branch', 'other', self.old + '^')
        self.git('switch', '-q', 'other')
        self.file.write_text('other branch\n')
        self.commit('other')
        other = self.rev('HEAD')
        self.git('switch', '-q', 'lab/dev-test')
        self.git('merge', '--no-edit', 'other', check=False)
        conflict = self.file.read_bytes()
        with self.assertRaisesRegex(ValueError, 'different merge'):
            sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual(other, self.rev('MERGE_HEAD'))
        self.assertEqual(conflict, self.file.read_bytes())

    def test_amend_removes_old_added_file(self):
        self.git('switch', '-q', 'feature/demo')
        added = self.root / 'old-only.txt'
        added.write_text('old feature addition\n')
        self.commit('old addition')
        previous = self.rev('HEAD')
        self.git('switch', '-q', 'lab/dev-test')
        self.git('merge', '--no-edit', 'feature/demo')
        self.git('switch', '-q', 'feature/demo')
        self.git('rm', 'old-only.txt')
        self.git('commit', '--amend', '--allow-empty', '-qm', 'amend removes addition')
        self.git('switch', '-q', 'lab/dev-test')
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertFalse(added.exists())
        self.assertEqual('lab protection\n', self.guard.read_text())

    def test_explicit_previous_is_checked(self):
        self.amend()
        with self.assertRaisesRegex(ValueError, 'not part of this integration'):
            sync_dev.sync_feature(self.root, 'feature/demo', force=True, previous=self.new)
        sync_dev.sync_feature(self.root, 'feature/demo', force=True, previous=self.old)
        self.assertEqual('version two\n', self.file.read_text())

    def test_linked_worktree_conflict_backup_uses_its_own_index(self):
        self.amend()
        linked = self.root / 'linked'
        self.git('worktree', 'add', '-b', 'lab/dev-copy', str(linked), 'HEAD')
        sync_dev.git(linked, 'merge', '--no-edit', 'feature/demo', check=False)
        backup = sync_dev.sync_feature(linked, 'feature/demo', force=True)
        self.assertEqual('version two\n', (linked / 'feature.txt').read_text())
        self.assertEqual('version one\nstale line\n', self.file.read_text())
        self.assertIn((self.root / '.git').resolve(), backup.parents)
        self.assertTrue((backup / 'index').is_file())

    def test_force_refuses_integration_mode_change(self):
        self.git('config', 'core.filemode', 'true')
        self.file.chmod(0o755)
        self.commit('integration executable bit')
        self.amend()
        with self.assertRaisesRegex(ValueError, 'integration changes'):
            sync_dev.sync_feature(self.root, 'feature/demo', force=True)

    def test_amend_adds_new_file(self):
        self.amend()
        self.git('switch', '-q', 'feature/demo')
        new = self.root / 'new-only.txt'
        new.write_text('new addition\n')
        self.git('add', '.')
        self.git('commit', '--amend', '-qm', 'amend adds file')
        self.git('switch', '-q', 'lab/dev-test')
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual('new addition\n', new.read_text())
        self.assertEqual('version two\n', self.file.read_text())

    def test_plain_sync_keeps_conflict_for_force_retry(self):
        self.amend()
        with self.assertRaises(ValueError):
            sync_dev.sync_feature(self.root, 'feature/demo')
        self.assertEqual(self.new, self.rev('MERGE_HEAD'))
        sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual('version two\n', self.file.read_text())

    def test_commit_hook_failure_is_not_bypassed(self):
        self.amend()
        hooks = self.root / '.git' / 'test-hooks'
        hooks.mkdir()
        hook = hooks / 'pre-commit'
        hook.write_text('#!/bin/sh\nexit 1\n')
        hook.chmod(0o755)
        self.git('config', 'core.hooksPath', str(hooks))
        head = self.rev('HEAD')
        with self.assertRaises(ValueError):
            sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        self.assertEqual(head, self.rev('HEAD'))
        self.assertEqual(self.new, self.rev('MERGE_HEAD'))
        self.assertEqual('version two\n', self.file.read_text())

    def test_source_can_be_amended_again_while_merge_is_pending(self):
        self.amend()
        self.git('merge', '--no-edit', 'feature/demo', check=False)
        pending = self.new
        source_dir = tempfile.TemporaryDirectory()
        self.addCleanup(source_dir.cleanup)
        checkout = Path(source_dir.name) / 'feature'
        self.git('worktree', 'add', str(checkout), 'feature/demo')
        (checkout / 'feature.txt').write_text('newer amend\n')
        self.git('-C', str(checkout), 'add', '.')
        self.git('-C', str(checkout), 'commit', '--amend', '-qm', 'newer amend')
        backup = sync_dev.sync_feature(self.root, 'feature/demo', force=True)
        manifest = json.loads((backup / 'manifest.json').read_text())
        self.assertEqual([pending], manifest['pending_heads'])
        self.assertEqual('newer amend\n', self.file.read_text())

    def test_notes_branch_cannot_be_merged_into_code(self):
        self.git('switch', '--orphan', 'notes-test')
        (self.root / 'notes.md').write_text('notes only\n')
        self.commit('notes')
        self.git('switch', '-q', 'lab/dev-test')
        head = self.rev('HEAD')
        with self.assertRaisesRegex(ValueError, 'code feature'):
            sync_dev.sync_feature(self.root, 'notes-test', force=True, previous=self.old)
        self.assertEqual(head, self.rev('HEAD'))
