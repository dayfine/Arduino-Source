import unittest

from devtools.code_health.findings import Finding, annotation, annotations
from devtools.code_health.metrics import Metric
from devtools.code_health.policy import advisories


class AnnotationTests(unittest.TestCase):
    def test_error_and_warning_carry_file_and_line(self):
        finding = Finding('Common/a.cpp', 12, 'f(): function_lines=60')
        self.assertEqual('Common/a.cpp:12: f(): function_lines=60', str(finding))
        self.assertEqual('::error file=Common/a.cpp,line=12,title=Code health::f(): function_lines=60',
                         annotation('error', finding))
        self.assertEqual('::warning file=Common/a.cpp,line=12,title=Code health (advisory)::'
                         'f(): function_lines=60', annotation('warning', finding))

    def test_message_and_properties_are_escaped(self):
        finding = Finding('Common/a,b:c%.cpp', 3, '100% done\r\nnext: a,b')
        self.assertEqual('::error file=Common/a%2Cb%3Ac%25.cpp,line=3,title=Code health::'
                         '100%25 done%0D%0Anext: a,b', annotation('error', finding))

    def test_cap_annotates_first_findings_and_counts_the_rest(self):
        findings = [Finding('Common/a.cpp', line, 'x') for line in range(1, 8)]
        lines = annotations('warning', findings, cap=5)
        self.assertEqual(6, len(lines))
        self.assertTrue(lines[4].startswith('::warning file=Common/a.cpp,line=5,'))
        self.assertEqual('Annotated 5 of 7 code health (advisory) findings; '
                         'see the log above for the rest.', lines[5])
        self.assertEqual(3, len(annotations('error', findings[:3], cap=5)))


class AdvisoryLocationTests(unittest.TestCase):
    PATH = 'Common/a.cpp'

    def locations(self, metrics, previous=()):
        return [(f.path, f.line, f.message.split(' (')[0]) for f in advisories(metrics, list(previous))]

    def test_long_changed_file_is_reported_at_line_one(self):
        self.assertEqual([(self.PATH, 1, '<file>: file_lines=600')],
                         self.locations([Metric(self.PATH, '', 'file_lines', 600, 1000)]))
        same_length = Metric(self.PATH, '', 'file_lines', 600, 1000)
        self.assertEqual(1, len(self.locations([same_length], [same_length])))
        self.assertEqual([], self.locations([Metric(self.PATH, '', 'file_lines', 500, 1000)]))

    def test_function_between_25_and_50_lines_is_reported_at_its_start(self):
        metrics = [Metric(self.PATH, 'f()', 'function_lines', 26, 50, 40),
                   Metric(self.PATH, 'g()', 'function_lines', 25, 50, 90),
                   Metric(self.PATH, 'h()', 'function_lines', 51, 50, 120)]
        self.assertEqual([(self.PATH, 40, 'f(): function_lines=26')], self.locations(metrics))
        self.assertEqual([], self.locations(metrics[:1], metrics[:1]))

    def test_class_with_six_or_seven_public_methods_is_reported_at_the_class(self):
        metrics = [Metric(self.PATH, 'A', 'public_methods', 6, 7, 7),
                   Metric(self.PATH, 'B', 'public_methods', 7, 7, 30),
                   Metric(self.PATH, 'C', 'public_methods', 5, 7, 50)]
        self.assertEqual([(self.PATH, 7, 'A: public_methods=6'), (self.PATH, 30, 'B: public_methods=7')],
                         self.locations(metrics))


if __name__ == '__main__':
    unittest.main()
