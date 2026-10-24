import datetime
import unittest

from devtools.code_health.metrics import Metric
from devtools.code_health.policy import evaluate, make_baseline


TODAY = datetime.date(2026, 10, 24)


def function(value):
    return Metric('Common/test.cpp', 'test()', 'function_lines', value, 50)


class RatchetTests(unittest.TestCase):
    def test_current_debt_passes_but_growth_fails(self):
        baseline = make_baseline([function(70)], '2027-01-22')
        self.assertEqual([], evaluate([function(70)], baseline, TODAY))
        self.assertTrue(evaluate([function(71)], baseline, TODAY))

    def test_improved_function_cannot_regrow_to_old_baseline(self):
        baseline = make_baseline([function(90)], '2027-01-22')
        self.assertTrue(evaluate([function(80)], baseline, TODAY,
                                 previous=[function(70)]))

    def test_new_violation_fails(self):
        baseline = make_baseline([], '2027-01-22')
        self.assertTrue(evaluate([function(51)], baseline, TODAY))

    def test_expired_exception_fails_only_while_still_needed(self):
        baseline = make_baseline([function(70)], '2026-10-23')
        self.assertTrue(evaluate([function(60)], baseline, TODAY))
        self.assertEqual([], evaluate([function(50)], baseline, TODAY))

    def test_missing_review_date_is_rejected(self):
        baseline = make_baseline([function(70)], '2027-01-22')
        del baseline['violations'][0]['review_at']
        with self.assertRaises(ValueError):
            evaluate([function(60)], baseline, TODAY)


if __name__ == '__main__':
    unittest.main()
