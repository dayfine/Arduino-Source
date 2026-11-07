"""Check formatter behavior: python3 -m unittest discover -s devtools/format -v."""

import os
from pathlib import Path
import shlex
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[2]
COMMAND = shlex.split(os.environ.get('UNCRUSTIFY', 'uncrustify'))


def format_cpp(source):
    result = subprocess.run(
        [*COMMAND, '-q', '-c', str(ROOT / '.uncrustify.cfg'), '-l', 'CPP'],
        input=source, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=True,
    )
    if result.stderr:
        raise RuntimeError(result.stderr.decode(errors='replace'))
    return result.stdout


class FormatterConfigTests(unittest.TestCase):
    def test_pinned_version_and_valid_configuration(self):
        version = subprocess.check_output([*COMMAND, '--version'], text=True)
        self.assertRegex(version.strip(), r'^Uncrustify-0\.83\.0(?:_f)?$')
        format_cpp(b'void f();\n')

    def test_exact_compact_braces_and_control_parentheses(self):
        source = b'namespace Example {\nvoid f() {\nif(ready()) {\nrun();\n} else {\nstop();\n}\n}\n}\n'
        expected = b'namespace Example{\nvoid f(){\n    if (ready()){\n        run();\n    }else{\n        stop();\n    }\n}\n}\n'
        self.assertEqual(expected, format_cpp(source))
        self.assertEqual(expected, format_cpp(expected))

    def test_class_members_access_labels_and_type_attached_pointers(self):
        source = b'class Thing{\npublic:\nvoid f(int *p){\n*p=1;\n}\n};\n'
        expected = b'class Thing{\npublic:\n    void f(int* p){\n        *p = 1;\n    }\n};\n'
        self.assertEqual(expected, format_cpp(source))

    def test_include_order_is_preserved_including_own_header_last(self):
        source = b'#include <vector>\n#include "Common/Cpp/Exceptions.h"\n\n#include "Own.h"\n'
        self.assertEqual(source, format_cpp(source))

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

    def test_braced_single_line_control_bodies_are_expanded(self):
        result = format_cpp(b'void f(){if (x){run();}else{stop();}}\n')
        self.assertIn(b'if (x){\n', result)
        self.assertIn(b'}else{\n', result)
        self.assertNotIn(b'{run();}', result)
        self.assertEqual(result, format_cpp(result))

    def test_templates_designated_initializers_and_raw_strings(self):
        source = (
            b'template <typename T>\nstruct Case{\n    T input;\n    T expectation;\n};\n'
            b'constexpr Case<int> cases[] = {\n    {.input = 1, .expectation = 2},\n};\n'
            b'const char* text = R"tag(if(x) { } else { /* keep */ })tag";\n'
        )
        result = format_cpp(source)
        self.assertIn(b'R"tag(if(x) { } else { /* keep */ })tag"', result)
        self.assertIn(b'.input = 1', result)
        self.assertIn(b'.expectation = 2', result)
        self.assertEqual(result, format_cpp(result))

    def test_macro_continuations_and_token_pasting(self):
        source = (
            b'#define JOIN(a, b) a ## b\n'
            b'#define TRACE(x) do { \\\n'
            b'    trace(#x, x); \\\n'
            b'} while (false)\n'
        )
        result = format_cpp(source)
        self.assertIn(b'a ## b', result)
        self.assertIn(b'trace(#x, x);', result)
        self.assertEqual(source.count(b'\\\n'), result.count(b'\\\n'))
        self.assertEqual(result, format_cpp(result))

    def test_constructor_comment_stays_before_body_brace(self):
        source = (
            b'Thing::Thing()\n    : m_value(0)\n'
            b'//    , m_other(1)\n{\n    run();\n}\n'
        )
        result = format_cpp(source)
        self.assertIn(b'//    , m_other(1)\n{', result)
        self.assertEqual(result, format_cpp(result))

    def test_known_inherited_class_spacing_gap(self):
        # Unlike control/namespace braces, this case is not fully configurable.
        source = b'struct Derived : public Base{\n    int value;\n};\n'
        self.assertIn(b'struct Derived : public Base {', format_cpp(source))

    def test_conversion_operators_and_nested_templates_preserve_tokens(self):
        source = (
            b'template <typename Type>\nstruct Holder{\n'
            b'    operator const Type&() const{ return value; }\n'
            b'    Type value;\n};\n'
            b'std::vector<std::unique_ptr<Holder<int>>> values;\n'
        )
        result = format_cpp(source)
        self.assertIn(b'operator const Type&()', result)
        self.assertNotIn(b'constType', result)
        self.assertIn(b'Holder<int>>>', result)
        self.assertEqual(result, format_cpp(result))
