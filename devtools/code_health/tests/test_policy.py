import unittest

from devtools.code_health.metrics import Metric, measure
from devtools.code_health.policy import evaluate


def function(value, symbol='test()'):
    return Metric('Common/test.cpp', symbol, 'function_lines', value, 50)


class RatchetTests(unittest.TestCase):
    def test_existing_debt_passes_but_growth_fails(self):
        self.assertEqual([], evaluate([function(70)], [function(70)]))
        self.assertTrue(evaluate([function(71)], [function(70)]))

    def test_improvement_is_allowed(self):
        self.assertEqual([], evaluate([function(60)], [function(70)]))

    def test_new_function_must_meet_limit(self):
        self.assertTrue(evaluate([function(51, 'new()')], [function(70)]))

    def test_unchanged_bad_function_does_not_block_other_changes(self):
        self.assertEqual([], evaluate([function(70), function(25, 'new()')], [function(70)]))

    def test_changed_parameter_types_keep_the_base(self):
        old = measure('Common/test.cpp', 'void A::f(std::function<void()> f){\n' + 'x();\n' * 60 + '}\n')
        new = measure('Common/test.cpp', 'void A::f(MoveOnlyFunction<void()> f){\n' + 'x();\n' * 60 + '}\n')
        self.assertEqual([], evaluate(new, old))
        grown = measure('Common/test.cpp', 'void A::f(MoveOnlyFunction<void()> f){\n' + 'x();\n' * 61 + '}\n')
        self.assertTrue(evaluate(grown, old))

    def test_growth_below_limit_is_allowed(self):
        self.assertEqual([], evaluate([function(50)], [function(25)]))


if __name__ == '__main__':
    unittest.main()
