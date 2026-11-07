"""Check the formatter proposal: python3 -m unittest discover -s devtools/format -v."""

import os
from pathlib import Path
import shlex
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]
COMMAND = shlex.split(os.environ.get('CLANG_FORMAT', 'clang-format'))


def format_cpp(source):
    return subprocess.run(
        [*COMMAND, '--style=file', '--assume-filename=sample.cpp'],
        input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        cwd=ROOT, check=True,
    ).stdout


class FormatterConfigTests(unittest.TestCase):
    def test_pinned_version_and_valid_configuration(self):
        result = subprocess.check_output([*COMMAND, '--version'], text=True)
        self.assertIn('version 21.1.8', result)
        subprocess.run([*COMMAND, '--style=file', '--dump-config'], cwd=ROOT,
                       stdout=subprocess.DEVNULL, check=True)

    def test_indentation_pointer_alignment_and_multiline_control_flow(self):
        result = format_cpp(b'void f(int *p){\nif(p){\n*p=1;\n}else{\n*p=2;\n}\n}\n')
        self.assertIn(b'void f(int* p)', result)
        self.assertIn(b'    if (p)', result)
        self.assertIn(b'        *p = 1;', result)
        self.assertNotIn(b'\t', result)

    def test_include_order_is_preserved_including_own_header_last(self):
        source = b'#include <vector>\n#include "Common/Cpp/Exceptions.h"\n\n#include "Own.h"\n'
        result = format_cpp(source)
        self.assertEqual([s for s in source.splitlines() if s.startswith(b'#include')],
                         [s for s in result.splitlines() if s.startswith(b'#include')])

    def test_existing_lf_and_crlf_are_preserved(self):
        for newline in (b'\n', b'\r\n'):
            with self.subTest(newline=newline):
                source = newline.join([b'void f(){', b'    g();', b'}', b''])
                result = format_cpp(source)
                if newline == b'\r\n':
                    self.assertNotIn(b'\n', result.replace(b'\r\n', b''))
                else:
                    self.assertNotIn(b'\r', result)

    def test_comments_and_long_lines_are_not_reflowed(self):
        comment = b'// ' + b' '.join([b'keep this explanation intact'] * 8)
        code = b'    call(' + b', '.join(b'argument_' + str(i).encode() for i in range(25)) + b');'
        result = format_cpp(b'void f(){\n' + comment + b'\n' + code + b'\n}\n')
        self.assertIn(comment, result)
        self.assertIn(code, result)

    def test_known_brace_spacing_gap_and_idempotence(self):
        # Deliberately exposes the incompatibility, rather than claiming parity.
        source = b'namespace Sample{\nvoid f(){\nif (ready()){\nrun();\n}else{\nstop();\n}\n}\n}\n'
        result = format_cpp(source)
        self.assertIn(b'namespace Sample {', result)
        self.assertIn(b'if (ready()) {', result)
        self.assertIn(b'} else {', result)
        self.assertEqual(result, format_cpp(result))
