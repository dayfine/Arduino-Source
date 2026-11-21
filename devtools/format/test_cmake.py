"""CMake format targets: python3 -m unittest discover -s devtools/format -p test_cmake.py -v."""

from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


class CMakeFormattingTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='format targets ')
        self.addCleanup(self.directory.cleanup)
        root = Path(self.directory.name)
        self.repo, self.build, self.project = root / 'code tree', root / 'build tree', root / 'project'
        self.project.mkdir()
        (self.repo / 'Common').mkdir(parents=True)
        (self.repo / 'devtools/format').mkdir(parents=True)
        shutil.copy2(ROOT / '.uncrustify.cfg', self.repo / '.uncrustify.cfg')
        shutil.copy2(ROOT / 'devtools/format/format_changed.py', self.repo / 'devtools/format/format_changed.py')
        self.source = self.repo / 'Common/example.cpp'
        self.source.write_bytes(b'void f(){\n    int untouched=1;\n    int changed=2;\n}\n')
        self.git('init', '-q')
        self.git('add', '.')
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                 '-c', 'commit.gpgSign=false', '-c', 'core.hooksPath=/dev/null', 'commit', '-qm', 'base')
        self.marker = root / 'app-was-built'
        self.project.joinpath('CMakeLists.txt').write_text(
            'cmake_minimum_required(VERSION 3.24)\nproject(FormatTargets NONE)\n'
            f'include("{(ROOT / "SerialPrograms/cmake/Formatting.cmake").as_posix()}")\n'
            f'pa_add_format_targets("{self.repo.as_posix()}")\n'
            f'add_custom_target(app ALL COMMAND "${{CMAKE_COMMAND}}" -E touch "{self.marker.as_posix()}")\n'
        )

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.repo), *args])

    def configure(self, *args):
        subprocess.run(['cmake', '-S', str(self.project), '-B', str(self.build),
                        '-DPA_FORMAT_PYTHON=' + sys.executable, *args],
                       capture_output=True, check=True)

    def target(self, name):
        return subprocess.run(['cmake', '--build', str(self.build), '--target', name], capture_output=True)

    def test_check_write_and_no_application_dependency(self):
        self.configure()
        self.source.write_bytes(b'void f(){\n    int untouched=1;\n    int changed=3;\n}\n')
        staged = self.git('diff', '--cached', '--binary')
        self.assertNotEqual(0, self.target('format-check').returncode)
        result = self.target('format')
        self.assertEqual(0, result.returncode, result.stderr.decode())
        self.assertEqual(b'void f(){\n    int untouched=1;\n    int changed = 3;\n}\n', self.source.read_bytes())
        self.assertEqual(0, self.target('format-check').returncode)
        self.assertEqual(staged, self.git('diff', '--cached', '--binary'))
        self.assertFalse(self.marker.exists())

    def test_missing_python_does_not_break_configure(self):
        self.configure('-DPA_FORMAT_PYTHON=' + str(self.repo / 'missing-python'))
        result = self.target('format')
        self.assertNotEqual(0, result.returncode)
        self.assertIn(b'Formatting needs Python 3.8+', result.stderr)
        self.assertFalse(self.marker.exists())

    def test_configured_parent_includes_committed_edits(self):
        base = self.git('rev-parse', 'HEAD').decode().strip()
        self.source.write_bytes(b'void f(){\n    int untouched=1;\n    int changed=3;\n}\n')
        self.git('add', '.')
        self.git('-c', 'user.name=Test', '-c', 'user.email=test@example.invalid',
                 '-c', 'commit.gpgSign=false', '-c', 'core.hooksPath=/dev/null', 'commit', '-qm', 'edit')
        self.configure('-DPA_FORMAT_BASE=' + base)
        self.assertEqual(0, self.target('format').returncode)
        self.assertIn(b'int changed = 3;', self.source.read_bytes())
        self.assertIn(b'int untouched=1;', self.source.read_bytes())
