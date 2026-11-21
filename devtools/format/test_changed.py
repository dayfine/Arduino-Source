"""Incremental formatting tests: python3 -m unittest discover -s devtools/format -v."""

from contextlib import redirect_stderr, redirect_stdout
import io
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import format_changed as changed


class LocalEditTests(unittest.TestCase):
    def test_only_selected_line_changes_and_indent_is_preserved(self):
        source = b'    int before=1;\n          int value=2;\n    int after=3;\n'
        formatted = b'    int before = 1;\n    int value = 2;\n    int after = 3;\n'
        candidate, _ = changed.local_edits(source, formatted, {2})
        self.assertEqual(b'    int before=1;\n          int value = 2;\n    int after=3;\n', candidate)

    def test_reflows_and_declaration_braces_are_skipped(self):
        source = b'class Derived : public Base{\n    void f(){ if(x){run();} }\n};\n'
        formatted = b'class Derived : public Base {\n    void f(){\n        if (x){\n            run();\n        }\n    }\n};\n'
        candidate, skips = changed.local_edits(source, formatted, {1, 2})
        self.assertEqual(source, candidate)
        self.assertTrue(skips)
        declaration = b'class Derived : public Base{\n'
        candidate, skips = changed.local_edits(
            declaration, b'class Derived : public Base {\n', {1}
        )
        self.assertEqual(declaration, candidate)
        self.assertEqual({'protected': 1}, skips)

    def test_preprocessor_and_continuation_lines_are_not_edited(self):
        source = b'#define M(x) \\\n    use(x)\nvoid f(){\n    run();\n}\n'
        formatted = b'#define M(x) \\\n    use (x)\nvoid f(){\n    run ();\n}\n'
        candidate, skips = changed.local_edits(source, formatted, {1, 2, 4})
        self.assertEqual(b'#define M(x) \\\n    use(x)\nvoid f(){\n    run ();\n}\n', candidate)
        self.assertTrue(skips)

    def test_crlf_and_deletion_only_ranges(self):
        candidate, _ = changed.local_edits(b'int x=1;\r\n', b'int x = 1;\r\n', {1})
        self.assertEqual(b'int x = 1;\r\n', candidate)
        self.assertEqual(set(), changed.hunk_lines(b'@@ -2 +1,0 @@\n-deleted\n'))
        self.assertEqual({3, 4}, changed.hunk_lines(b'@@ -2 +3,2 @@\n+x\n+y\n'))

    def test_no_nonwhitespace_rewrite_is_accepted(self):
        with self.assertRaisesRegex(ValueError, 'non-whitespace'):
            changed.local_edits(b'int x=1;\n', b'int x = 2;\n', {1})


class TokenGuardTests(unittest.TestCase):
    def test_identifier_merge_and_literal_changes_are_rejected(self):
        for before, after in [
            (b'operator const Type&();\n', b'operator constType&();\n'),
            (b'const char* s = "a b";\n', b'const char* s = "ab";\n'),
            (b'const char* s = R"x(a b)x";\n', b'const char* s = R"x(ab)x";\n'),
            (b'int x = a + +b;\n', b'int x = a++b;\n'),
            (b'const char* s = R"x(a\nb)x";\n', b'const char* s = R"x(ab)x";\n'),
        ]:
            with self.subTest(before=before):
                with self.assertRaisesRegex(ValueError, 'token'):
                    changed.verify_tokens(before, after, 'clang')

    def test_whitespace_in_code_preserves_tokens(self):
        changed.verify_tokens(b'if(x){run();}\n', b'if (x){run();}\n', 'clang')


class GitIntegrationTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.root = Path(self.directory.name)
        self.path = self.root / 'Common/example.cpp'
        self.path.parent.mkdir()
        self.path.write_bytes(b'void f(){\n    int old=1;\n    int value=2;\n}\n')
        self.git('init', '-q')
        self.git('add', '.')
        self.git('-c', 'user.name=Formatter Test', '-c', 'user.email=test@example.invalid',
                 '-c', 'core.hooksPath=/dev/null', 'commit', '-qm', 'base')

    def git(self, *args):
        return subprocess.check_output(['git', '-C', str(self.root), *args])

    def cli(self, args):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            return changed.main(args)

    def test_default_preview_check_and_write_leave_index_untouched(self):
        self.path.write_bytes(b'void f(){\n    int old=1;\n    int value=3;\n}\n')
        self.git('add', '.')
        staged = self.git('diff', '--cached', '--binary')
        source = self.path.read_bytes()
        common = ['--repo', str(self.root)]
        self.assertEqual(0, self.cli(common))
        self.assertEqual(source, self.path.read_bytes())
        self.assertEqual(1, self.cli([*common, '--check']))
        self.assertEqual(0, self.cli([*common, '--write']))
        self.assertEqual(b'void f(){\n    int old=1;\n    int value = 3;\n}\n', self.path.read_bytes())
        self.assertEqual(staged, self.git('diff', '--cached', '--binary'))
        self.assertEqual(0, self.cli([*common, '--check']))

    def test_source_change_after_plan_is_refused(self):
        plan = changed.Plan(self.path, self.path.read_bytes(), b'new text', {})
        self.path.write_bytes(b'user edit')
        with self.assertRaisesRegex(ValueError, 'changed'):
            changed.write_plans([plan])
        self.assertEqual(b'user edit', self.path.read_bytes())

    def test_untracked_and_rename_only_files_are_not_formatted(self):
        other = self.root / 'Common/new.cpp'
        other.write_bytes(b'int untouched=0;\n')
        self.assertEqual({}, changed.selected_lines(self.root, 'HEAD'))
        self.git('mv', 'Common/example.cpp', 'Common/renamed.cpp')
        self.assertEqual({}, changed.selected_lines(self.root, 'HEAD'))

    def test_filename_with_spaces_and_pathspec_characters(self):
        path = self.root / 'Common/new [case].cpp'
        path.write_bytes(b'int value=3;\n')
        self.git('add', '.')
        self.assertEqual(0, self.cli(['--repo', str(self.root), '--write']))
        self.assertEqual(b'int value = 3;\n', path.read_bytes())

    def test_invalid_second_candidate_prevents_all_writes(self):
        self.path.write_bytes(b'int first=1;\n')
        other = self.root / 'Common/second.cpp'
        other.write_bytes(b'int second=2;\n')
        self.git('add', '.')
        before = self.path.read_bytes(), other.read_bytes()
        with patch.object(changed, 'verify_tokens', side_effect=[None, ValueError('token mismatch')]):
            self.assertEqual(2, self.cli(['--repo', str(self.root), '--write']))
        self.assertEqual(before, (self.path.read_bytes(), other.read_bytes()))

    def test_snapshot_change_after_diff_selection_is_refused(self):
        self.path.write_bytes(b'int value=3;\n')
        selection = changed.selected_lines(self.root, 'HEAD')
        self.path.write_bytes(b'user changed the file')
        with self.assertRaisesRegex(ValueError, 'changed after measuring'):
            changed.make_plans(self.root, selection, 'uncrustify', 'clang')

    def test_explicit_base_includes_committed_changes(self):
        base = self.git('rev-parse', 'HEAD').decode().strip()
        self.path.write_bytes(b'void f(){\n    int old=1;\n    int value=3;\n}\n')
        self.git('add', '.')
        self.git('-c', 'user.name=Formatter Test', '-c', 'user.email=test@example.invalid',
                 '-c', 'core.hooksPath=/dev/null', 'commit', '-qm', 'edit')
        self.assertEqual({}, changed.selected_lines(self.root, 'HEAD'))
        self.assertEqual(0, self.cli(['--repo', str(self.root), '--base', base, '--write']))
        self.assertEqual(b'void f(){\n    int old=1;\n    int value = 3;\n}\n', self.path.read_bytes())
