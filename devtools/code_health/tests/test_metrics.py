import unittest
from pathlib import Path

from devtools.code_health.metrics import measure


class MetricsTests(unittest.TestCase):
    def test_small_function_passes(self):
        metrics = measure('Common/small.cpp', 'int small(int x){ return x; }\n')
        self.assertEqual([], [m for m in metrics if m.value > m.limit])

    def test_threshold_fixtures(self):
        fixtures = Path(__file__).with_name('fixtures')
        for check in ('file', 'function', 'nesting', 'parameters'):
            for verdict, expected in (('pass', False), ('fail', True)):
                name = f'{check}_{verdict}.cpp'
                with self.subTest(fixture=name):
                    values = measure(name, (fixtures / name).read_text())
                    violations = [m for m in values if m.value > m.limit]
                    self.assertEqual(expected, bool(violations), violations)

    def test_strings_comments_and_raw_literals_do_not_add_nesting(self):
        source = r'''void strings(){
    // {{{{{{{{
    const char* a = "{{{{{{{{";
    const char* b = R"tag({{{{{{{{)tag";
    /* {{{{{{{{ */
}'''
        nesting = [m.value for m in measure('literals.cpp', source)
                   if m.check == 'nesting']
        self.assertEqual([0], nesting)

    def test_qualified_functions_and_lambdas_are_measured(self):
        source = 'namespace n{ struct A{ void f(){ auto g = [](){ if (true){} }; } }; }'
        metrics = measure('methods.hpp', source)
        self.assertTrue(any(m.symbol.endswith('A::f()') for m in metrics), metrics)
        self.assertEqual([2], [m.value for m in metrics if m.check == 'nesting'])

    def test_crlf_macro_header_has_identical_metrics_to_lf(self):
        header = Path(__file__).resolve().parents[3] / 'SerialPrograms/Source/Kernels/Kernels_x64_AVX512.h'
        source = header.read_text()
        self.assertEqual(measure('macro.h', source),
                         measure('macro.h', source.replace('\n', '\r\n')))


if __name__ == '__main__':
    unittest.main()
