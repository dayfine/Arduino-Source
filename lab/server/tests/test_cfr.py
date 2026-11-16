"""The cfr_batch allowlist (pa_lab/cfr.py): which lines pass, what each may cost, and the budget.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import unittest

from pa_lab import cfr, control


class AllowedLineTests(unittest.TestCase):
    def test_allowed_lines_and_their_worst_case(self):
        cases = [
            dict(name='press with defaults', input='press A', expectation=160 + 160 + 200),
            dict(name='press two buttons, hold and release', input='press L+R 100 1500', expectation=1800),
            dict(name='HOME is a button', input='press HOME 100 1000', expectation=1300),
            dict(name='dpad with defaults', input='dpad DOWN', expectation=100 + 200 + 200),
            dict(name='dpad holding B', input='dpad UP 160 400 B', expectation=760),
            dict(name='stick with a button', input='stick L 0 1 1000 160 B', expectation=1360),
            dict(name='stick with decimals', input='stick R -0.5 0.25', expectation=520),
            dict(name='wait', input='wait 1500', expectation=1700),
            dict(name='taps counts every tap', input='taps LEFT 3', expectation=3 * (160 + 640 + 200) + 200),
            dict(name='taps all with timings', input='taps RIGHT 2 all 80 400 12.5', expectation=2 * 680 + 200),
            dict(name='walk default max', input='walk 0 1', expectation=5000 + 400 + 200),
            dict(name='walk with max, still and B', input='walk -1 0 2000 10 B', expectation=2600),
            dict(name='shot', input='shot after_a', expectation=1200),
            dict(name='shots', input='shots door 3 250', expectation=3 * 1250 + 200),
            dict(name='detect', input='detect home', expectation=1200),
            dict(name='wait_for default timeout', input='wait_for black', expectation=6200),
            dict(name='expect with timeout', input='expect home 3000', expectation=4200),
            dict(name='game_open', input='game_open', expectation=1200),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.assertEqual(cfr.check_line(case['input']), case['expectation'])


class RefusedLineTests(unittest.TestCase):
    def test_lines_outside_the_allowlist(self):
        cases = [
            dict(name='channel: start', input='start', expectation='not an allowed'),
            dict(name='channel: select', input='select PokemonBDSP:AutoStory', expectation='not an allowed'),
            dict(name='channel: set', input='set START_SEGMENT "0"', expectation='not an allowed'),
            dict(name='channel: stop', input='stop', expectation='not an allowed'),
            dict(name='channel: owner release', input='owner release', expectation='not an allowed'),
            dict(name='channel: a second request id', input='request 3e8a6c21-7f90-4b2d-a1c4-6d0e9b5f8a73',
                 expectation='not an allowed'),
            dict(name='channel: keepawake', input='keepawake off', expectation='not an allowed'),
            dict(name='channel: popups', input='popups close', expectation='not an allowed'),
            dict(name='close_game', input='close_game', expectation='not an allowed'),
            dict(name='history', input='history run', expectation='not an allowed'),
            dict(name='snapshot', input='snapshot frame 2000', expectation='not an allowed'),
            dict(name='state', input='state', expectation='not an allowed'),
            dict(name='a comment', input='# press A', expectation='not an allowed'),
            dict(name='blank', input='', expectation='canonical'),
            dict(name='two spaces', input='press  A', expectation='canonical'),
            dict(name='trailing space', input='press A ', expectation='canonical'),
            dict(name='a tab', input='press\tA', expectation='canonical'),
            dict(name='a newline smuggles a second command', input='press A\nstart', expectation='canonical'),
            dict(name='CAPTURE button', input='press CAPTURE', expectation='buttons'),
            dict(name='repeated button', input='press A+A', expectation='buttons'),
            dict(name='lower-case button', input='press a', expectation='buttons'),
            dict(name='hold too long', input='press A 10001', expectation='hold'),
            dict(name='negative release', input='press A 100 -1', expectation='release'),
            dict(name='leading zero', input='wait 0100', expectation='ms'),
            dict(name='too many arguments', input='press A 1 2 3', expectation='usage'),
            dict(name='bad direction', input='dpad NORTH', expectation='direction'),
            dict(name='stick out of range', input='stick L 1.5 0', expectation='x must'),
            dict(name='stick in exponent form', input='stick L 1e0 0', expectation='x must'),
            dict(name='wait too long', input='wait 30001', expectation='ms'),
            dict(name='too many taps', input='taps UP 21', expectation='N must'),
            dict(name='walk max too long', input='walk 0 1 10001', expectation='max'),
            dict(name='shot label with a slash', input='shot ../x', expectation='label'),
            dict(name='too many shots', input='shots s 11', expectation='count'),
            dict(name='detect on an image path', input='detect home /tmp/x.png', expectation='usage'),
            dict(name='detector with a path', input='detect ../home', expectation='detector'),
            dict(name='game_open on an image', input='game_open image', expectation='usage'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                with self.assertRaises(ValueError) as caught:
                    cfr.check_line(case['input'])
                self.assertIn(case['expectation'], str(caught.exception))


class BatchTests(unittest.TestCase):
    def test_the_first_bad_line_is_named(self):
        with self.assertRaises(control.Refused) as caught:
            cfr.check(['press A', 'start', 'stop'])
        self.assertEqual(caught.exception.refusal['code'], 'invalid_arguments')
        self.assertEqual(caught.exception.refusal['field'], 'arguments.lines[1]')

    def test_a_batch_over_the_budget_is_refused(self):
        with self.assertRaises(control.Refused) as caught:
            cfr.check(['wait 30000', 'wait 30000'])
        self.assertEqual(caught.exception.refusal['field'], 'arguments.lines')
        self.assertEqual(caught.exception.refusal['evidence']['limit_ms'], cfr.BATCH_MAX_MS)

    def test_a_batch_within_the_budget_returns_its_worst_case(self):
        self.assertEqual(cfr.check(['shot before', 'press B 160 640', 'shot after']), 1200 + 1000 + 1200)


if __name__ == '__main__':
    unittest.main()
