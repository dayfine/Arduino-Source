"""Exercise changed-file discovery and real base comparisons in temporary Git repos."""
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from devtools.code_health.git_files import default_base, included


REPO = Path(__file__).resolve().parents[3]
# These fixtures' findings must not become annotations on the CI run's own PR.
ENV = {key: value for key, value in os.environ.items() if key != 'GITHUB_ACTIONS'}


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
                              cwd=REPO, capture_output=True, text=True, env=ENV)

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
                                cwd=REPO, capture_output=True, text=True, env=ENV)
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn(f'Base: {fork_point} (merge-base of HEAD and upstream/main).', result.stdout)


    def test_class_shape_applies_to_new_classes_only(self):
        interface = 'struct Listener{ virtual void on_event() = 0; };\n'
        (self.root / 'Common/old.h').write_text(interface)
        self.commit()
        with (self.root / 'Common/old.h').open('a') as file:
            file.write('int helper();\n')
        self.git('add', '.')
        self.assertEqual(0, self.cli().returncode)
        (self.root / 'Common/new.h').write_text(interface.replace('Listener', 'Fresh'))
        self.git('add', '.')
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn('Fresh: single_method_interface=1', result.stderr)
        self.assertIn('std::function', result.stderr)


    def test_shared_code_may_not_add_game_includes(self):
        shared = self.root / 'SerialPrograms/Source/CommonTools/Tool.cpp'
        shared.parent.mkdir(parents=True)
        shared.write_text('#include "Pokemon/Pokemon_Strings.h"\n')
        self.commit()
        shared.write_text('#include "Pokemon/Pokemon_Strings.h"\n#include "NintendoSwitch/X.h"\n')
        self.git('add', '.')
        self.assertEqual(0, self.cli().returncode)
        shared.write_text('#include "VideoGames/PokemonSV/PokemonSV_Settings.h"\n')
        self.git('add', '.')
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn('Tool.cpp:1: layering', result.stderr)

    def test_style_checks_report_only_added_lines(self):
        file = self.root / 'Common/style.cpp'
        file.write_text('void f(){\n    if (x) return;\n}\n')
        self.commit()
        file.write_text('void f(){\n    if (x) return;\n    g();\n}\n')
        self.git('add', '.')
        self.assertEqual(0, self.cli().returncode)
        file.write_text('void f(){\n    if (x) return;\n    std::mutex m;\n}\n')
        self.git('add', '.')
        result = self.cli()
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn('style.cpp:3: style: std::mutex', result.stderr)
        self.assertNotIn('style.cpp:2:', result.stderr)

    def test_github_annotates_failures_and_advisories(self):
        self.source(3)
        self.commit()
        self.source(30)
        self.assertNotIn('::warning', self.cli().stdout)
        result = self.cli('--github')
        self.assertEqual(0, result.returncode, result.stderr)
        self.assertIn('::warning file=Common/example.cpp,line=1,title=Code health (advisory)::'
                      'example(): function_lines=30', result.stdout)
        self.assertNotIn('::error', result.stdout)
        self.source(51)
        result = self.cli('--github')
        self.assertEqual(1, result.returncode, result.stderr)
        self.assertIn('::error file=Common/example.cpp,line=1,title=Code health::'
                      'example(): function_lines=51', result.stdout)
        self.assertIn('Code health: 1 failures.', result.stdout)


if __name__ == '__main__':
    unittest.main()
