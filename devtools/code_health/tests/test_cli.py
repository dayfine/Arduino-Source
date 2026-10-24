"""Exercise real Git discovery, persisted baselines, exit codes and base ratchets."""
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from devtools.code_health.git_files import included


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
        (self.root / 'devtools/code_health').mkdir(parents=True)

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args])

    def source(self, lines):
        (self.root / 'Common/example.cpp').write_text(
            'void example(){\n' + '    ;\n' * (lines - 2) + '}\n')
        self.git('add', '.')

    def cli(self, *args):
        return subprocess.run([sys.executable, '-m', 'devtools.code_health',
                               '--root', str(self.root), *args], cwd=REPO,
                              capture_output=True, text=True)

    def baseline(self):
        result = self.cli('--write-baseline', '--review-at', '2099-01-01')
        self.assertEqual(0, result.returncode, result.stderr)
        self.git('add', '.')
        self.git('commit', '-qm', 'initial debt')

    def test_baseline_and_regression_exit_codes(self):
        self.source(60)
        self.baseline()
        self.assertEqual(0, self.cli('--base-ref', 'HEAD').returncode)
        self.source(61)
        result = self.cli('--base-ref', 'HEAD')
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn('function_lines=61', result.stderr)

    def test_editing_baseline_cannot_hide_growth(self):
        self.source(60)
        self.baseline()
        self.source(61)
        self.assertEqual(0, self.cli('--write-baseline').returncode)
        self.assertEqual(1, self.cli('--base-ref', 'HEAD').returncode)

    def test_no_base_baseline_bootstrap_is_checked(self):
        self.source(60)
        self.git('commit', '-qm', 'before rollout')
        self.assertEqual(0, self.cli('--write-baseline').returncode)
        self.source(61)
        self.assertEqual(1, self.cli('--base-ref', 'HEAD').returncode)

    def test_untracked_and_third_party_files_are_excluded(self):
        self.source(3)
        self.baseline()
        (self.root / 'Common/untracked.cpp').write_text('// excess\n' * 1001)
        third_party = self.root / 'SerialPrograms/3rdParty-vendor/bad.cpp'
        third_party.parent.mkdir(parents=True)
        third_party.write_text('// excess\n' * 1001)
        self.git('add', str(third_party))
        self.assertEqual(0, self.cli('--base-ref', 'HEAD').returncode)

    def test_missing_or_malformed_baseline_fails_closed(self):
        self.source(3)
        self.assertEqual(2, self.cli().returncode)
        (self.root / 'devtools/code_health/baseline.json').write_text('{"schema":1}')
        self.assertEqual(2, self.cli().returncode)

    def test_cpp_extension_and_directory_scope(self):
        for extension in ('.h', '.hpp', '.c', '.cpp', '.tpp'):
            self.assertTrue(included('Common/sub/file' + extension))
        for path in ('3rdParty/a.cpp', 'Common/3rdParty2/a.cpp', 'other/a.cpp',
                     'Common/a.py', 'SerialPrograms/3rdParty/a.h'):
            self.assertFalse(included(path), path)


if __name__ == '__main__':
    unittest.main()
