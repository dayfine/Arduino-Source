import unittest

from devtools.code_health.metrics import measure


class MetricsTests(unittest.TestCase):
    def test_small_function_passes(self):
        metrics = measure('Common/small.cpp', 'int small(int x){ return x; }\n')
        self.assertEqual([], [m for m in metrics if m.value > m.limit])

    def test_generated_threshold_boundaries(self):
        cases = (
            ('file_lines', 1000, lambda n: '// line\n' * n),
            ('function_lines', 50, lambda n: 'void f(){\n' + '    ;\n' * (n - 2) + '}\n'),
            ('nesting', 4, lambda n: 'void f(){' + '{' * n + ';' + '}' * n + '}\n'),
            ('parameters', 7, lambda n: 'void f(' + ', '.join(f'int p{i}' for i in range(n)) + '){}\n'),
        )
        for check, limit, generate in cases:
            for size in (limit, limit + 1):
                with self.subTest(check=check, size=size):
                    metrics = [m for m in measure('generated.cpp', generate(size)) if m.check == check]
                    self.assertEqual([size], [m.value for m in metrics])
                    self.assertEqual(size > limit, any(m.value > m.limit for m in metrics))

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

    def test_crlf_macros_have_identical_metrics_to_lf(self):
        for count in (1, 4, 16, 64):
            parameters = ', '.join(f'p{i}' for i in range(count))
            source = '#define VALUES(' + parameters + ') \\\n    use(' + parameters + ')\n'
            source += 'void f(int a,\n int b){ return; }\n'
            self.assertEqual(measure('macro.h', source),
                             measure('macro.h', source.replace('\n', '\r\n')))


if __name__ == '__main__':
    unittest.main()
