"""Parse and place `// code-health: allow` tags; the policy tests cover what they permit."""
import unittest

from devtools.code_health.allow import attach, parse
from devtools.code_health.metrics import measure

PATH = 'Common/test.cpp'
WHY = 'a lookup table of game text; splitting it would scatter one table over files'


def body(lines):
    return '{\n' + '    x();\n' * (lines - 2) + '}\n'


class ParseTests(unittest.TestCase):
    def test_tag_lists_checks_and_justification(self):
        allowances, errors = parse(PATH, f'int a;\n// code-health: allow function_lines, nesting -- {WHY}\n')
        self.assertEqual([], errors)
        self.assertEqual(2, allowances[0].line)
        self.assertEqual(('function_lines', 'nesting'), allowances[0].checks)
        self.assertEqual(WHY, allowances[0].justification)

    def test_trailing_tag_on_a_code_line(self):
        allowances, errors = parse(PATH, f'void f(){{  // code-health: allow function_lines -- {WHY}\n')
        self.assertEqual([], errors)
        self.assertEqual(1, allowances[0].line)

    def test_failures(self):
        cases = (
            ('// code-health: allow function_lines', 'malformed'),
            ('// code-health: allow function_lines --', 'justification'),
            ('// code-health: ignore function_lines -- ' + WHY, 'malformed'),
            ('// code-health: allow line_length -- ' + WHY, 'unknown check line_length'),
            ('// code-health: allow function_lines -- legacy', 'justification'),
            ('// code-health: allow function_lines -- ' + 'x' * 60, 'justification'),
        )
        for text, expected in cases:
            with self.subTest(text=text):
                allowances, errors = parse(PATH, text + '\n')
                self.assertEqual([], allowances)
                self.assertEqual(1, len(errors))
                self.assertIn(expected, errors[0].message)


class AttachTests(unittest.TestCase):
    def attached(self, source):
        allowances, errors = parse(PATH, source)
        self.assertEqual([], errors)
        allowed, misplaced = attach(PATH, source, allowances, measure(PATH, source))
        return {m.symbol or '<file>': m.check for m in allowed}, misplaced

    def test_comment_block_and_template_above_the_signature(self):
        source = ('int a;\n\n'
                  f'// code-health: allow function_lines -- {WHY}\n'
                  '// More about f.\ntemplate <typename T>\nvoid f(T t)' + body(60))
        self.assertEqual(({'f( T t)': 'function_lines'}, []), self.attached(source))

    def test_tag_on_the_signature_line(self):
        source = 'int a;\n\nvoid f()' + body(60).replace('{', f'{{  // code-health: allow parameters -- {WHY}', 1)
        self.assertEqual(({'f()': 'parameters'}, []), self.attached(source))

    def test_tag_separated_by_code_attaches_to_nothing(self):
        source = f'// code-health: allow function_lines -- {WHY}\nint a;\nvoid f()' + body(60)
        allowed, misplaced = self.attached(source)
        self.assertEqual({}, allowed)
        self.assertIn('function_lines attaches to nothing', misplaced[0].message)

    def test_file_tag_only_near_the_top(self):
        tag = f'// code-health: allow file_lines -- {WHY}\n'
        self.assertEqual(({'<file>': 'file_lines'}, []), self.attached('int a;\n' * 19 + tag))
        allowed, misplaced = self.attached('int a;\n' * 20 + tag)
        self.assertEqual({}, allowed)
        self.assertIn('first 20 lines', misplaced[0].message)


if __name__ == '__main__':
    unittest.main()
