import unittest

from devtools.code_health.metrics import Metric, measure
from devtools.code_health.allow import Allowance
from devtools.code_health.policy import bypasses, evaluate


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

    def test_namespace_move_retains_only_an_unambiguous_removed_function_base(self):
        body = 'void f(){\n' + '    x();\n' * 60 + '}\n'
        old = measure('Common/test.cpp', body)
        moved = measure('Common/test.cpp', 'namespace Project{\nnamespace{\n' + body + '}\n}\n')
        self.assertEqual([], evaluate(moved, old))
        changed = measure('Common/test.cpp', 'namespace Project{\n' + body.replace('x();', 'y();') + '}\n')
        self.assertEqual([], evaluate(changed, old))
        grown = measure('Common/test.cpp', 'namespace Project{\n' + body.replace('x();\n', 'x();\ny();\n') + '}\n')
        self.assertTrue(evaluate(grown, old))
        copied = measure('Common/test.cpp', body + 'namespace Project{\n' + body + '}\n')
        self.assertTrue(evaluate(copied, old))

    def test_ambiguous_namespace_moves_do_not_transfer_debt(self):
        body = 'void f(){\n' + 'x();\n' * 60 + '}\n'
        old = measure('Common/test.cpp', body + 'namespace A{\n' + body + '}\n')
        new = measure('Common/test.cpp', 'namespace B{\n' + body + '}\n')
        self.assertTrue(evaluate(new, old))
        single_old = measure('Common/test.cpp', body)
        multiple_new = measure('Common/test.cpp', 'namespace B{\n' + body + '}\nnamespace C{\n' + body + '}\n')
        self.assertTrue(evaluate(multiple_new, single_old))

    def test_namespace_move_does_not_transfer_debt_across_files(self):
        body = 'void f(){\n' + 'x();\n' * 60 + '}\n'
        old = measure('Common/old.cpp', body)
        moved = measure('Common/new.cpp', 'namespace Project{\n' + body + '}\n')
        self.assertTrue(evaluate(moved, old))

    def test_growth_below_limit_is_allowed(self):
        self.assertEqual([], evaluate([function(50)], [function(25)]))


class BypassTests(unittest.TestCase):
    tag = Allowance(3, ('function_lines',), 'a lookup table of game text that reads best as one block')

    def test_tag_allows_up_to_twice_the_limit_and_reports_it(self):
        metric = function(100)
        self.assertEqual([], evaluate([metric], [], {metric: self.tag}))
        notices, unneeded = bypasses([metric], {metric: self.tag})
        self.assertEqual([], unneeded)
        self.assertIn('function_lines=100 over limit 50', notices[0].message)
        self.assertIn(self.tag.justification, notices[0].message)

    def test_tag_does_not_allow_more_than_twice_the_limit(self):
        metric = function(101)
        failures = evaluate([metric], [], {metric: self.tag})
        self.assertIn('allow tag permits at most 100', failures[0].message)

    def test_legacy_debt_above_the_ceiling_may_still_shrink(self):
        metric = function(110)
        self.assertEqual([], evaluate([metric], [function(120)], {metric: self.tag}))

    def test_yes_no_checks_have_no_ceiling(self):
        metric = Metric('Common/test.h', 'Callback', 'single_method_interface', 1, 0)
        self.assertEqual([], evaluate([metric], [], {metric: self.tag}))

    def test_tag_within_the_limit_is_flagged_as_unneeded(self):
        metric = function(40)
        notices, unneeded = bypasses([metric], {metric: self.tag})
        self.assertEqual([], notices)
        self.assertIn('remove its code-health allow tag', unneeded[0].message)
        self.assertEqual(3, unneeded[0].line)


if __name__ == '__main__':
    unittest.main()
