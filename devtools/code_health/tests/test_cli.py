"""Exercise changed-file discovery and real base comparisons in temporary Git repos."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from devtools.code_health.git_files import default_base, included


REPO = Path(__file__).resolve().parents[3]


class CliTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.git('init', '-q')
        self.git('config', 'user.email', 'test@example.invalid')
        self.git('config', 'user.name', 'Test')
        (self.root / 'Common').mkdir()

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args])

    def source(self, lines, path='Common/example.cpp'):
        file = self.root / path
        file.parent.mkdir(parents=True, exist_ok=True)
        file.write_text('void example(){\n' + '    ;\n' * (lines - 2) + '}\n')
        self.git('add', '.')

    def cli(self, *args):
        return subprocess.run([sys.executable, '-m', 'devtools.code_health',
                               '--root', str(self.root), '--base-ref', 'HEAD', *args],
                              cwd=REPO, capture_output=True, text=True)

    def commit(self):
        self.git('add', '.')
        self.git('commit', '-qm', 'base revision')

    def test_untouched_debt_is_not_scanned(self):
        self.source(60)
        self.commit()
        result = self.cli()
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('Scanned 0 changed C++ files', result.stdout)

    def test_existing_debt_can_improve_but_not_worsen(self):
        self.source(60)
        self.commit()
        self.source(59)
        self.assertEqual(0, self.cli().returncode)
        self.source(61)
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn('function_lines=61', result.stderr)
        self.assertIn('allowed 60', result.stderr)

    def test_new_file_must_meet_limits(self):
        self.source(3)
        self.commit()
        self.source(51, 'Common/new.hpp')
        self.assertEqual(1, self.cli().returncode)

    def test_unchanged_bad_function_in_changed_file_passes(self):
        self.source(60)
        self.commit()
        with (self.root / 'Common/example.cpp').open('a') as file:
            file.write('\nint small(){ return 0; }\n')
        self.assertEqual(0, self.cli().returncode)

    def test_rename_compares_with_original_path(self):
        self.source(60)
        self.commit()
        self.git('mv', 'Common/example.cpp', 'Common/renamed.cpp')
        result = self.cli()
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('Scanned 1 changed C++ files', result.stdout)

    def test_deletions_untracked_and_third_party_are_excluded(self):
        self.source(60)
        self.commit()
        self.git('rm', 'Common/example.cpp')
        (self.root / 'Common').mkdir(exist_ok=True)
        (self.root / 'Common/untracked.cpp').write_text('// excess\n' * 1001)
        self.source(60, 'SerialPrograms/3rdParty-vendor/bad.cpp')
        # source() stages everything; deliberately return the extra file to untracked.
        self.git('rm', '--cached', 'Common/untracked.cpp')
        result = self.cli()
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('Scanned 0 changed C++ files', result.stdout)

    def test_new_file_with_spaces_is_checked(self):
        self.source(3)
        self.commit()
        self.source(51, 'Common/new file.cpp')
        self.assertEqual(1, self.cli().returncode)

    def test_invalid_base_fails_closed(self):
        self.source(3)
        self.commit()
        self.assertEqual(2, self.cli('--base-ref', 'missing-ref').returncode)

    def test_cpp_extension_and_directory_scope(self):
        for extension in ('.h', '.hpp', '.c', '.cpp', '.tpp'):
            self.assertTrue(included('Common/sub/file' + extension))
        for path in ('3rdParty/a.cpp', 'Common/3rdParty2/a.cpp', 'other/a.cpp',
                     'Common/a.py', 'SerialPrograms/3rdParty/a.h'):
            self.assertFalse(included(path), path)


    def test_default_base_is_merge_base_with_upstream_then_origin(self):
        self.source(3)
        self.commit()
        fork_point = self.git('rev-parse', 'HEAD').decode().strip()
        with self.assertRaises(ValueError):
            default_base(self.root)
        self.git('update-ref', 'refs/remotes/origin/main', fork_point)
        self.source(60)
        self.commit()
        self.assertEqual((fork_point, 'merge-base of HEAD and origin/main'), default_base(self.root))
        # A stale fork main: upstream moved on from an older commit; prefer its merge-base.
        self.git('update-ref', 'refs/remotes/origin/main', 'HEAD')
        self.git('update-ref', 'refs/remotes/upstream/main', fork_point)
        self.assertEqual((fork_point, 'merge-base of HEAD and upstream/main'), default_base(self.root))
        result = subprocess.run([sys.executable, '-m', 'devtools.code_health', '--root', str(self.root)],
                                cwd=REPO, capture_output=True, text=True)
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn(f'Base: {fork_point} (merge-base of HEAD and upstream/main).', result.stdout)


if __name__ == '__main__':
    unittest.main()
