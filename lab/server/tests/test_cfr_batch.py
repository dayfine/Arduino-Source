"""cfr_batch against the fake app (Slice 5b, fork #131): a leased batch with the program stopped,
the save fact marked stale before anything is pressed, the journal's outcome, and the refusals
that send nothing.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import unittest

from pa_lab import contracts
from tests.control_fixtures import new_id
from tests.test_actions import CLIENT, ActionTest
from tests.test_preflight import save_known


def batch_request(token, lines, request_id=None):
    return {'schema': 1, 'request_id': request_id or new_id(), 'console': 'B', 'lease_token': token,
            'arguments': {'lines': lines}}


class CfrBatchTest(ActionTest):
    def cfr(self, request):
        return self.actions.cfr_batch(CLIENT, request)

    def batches_sent(self):
        return [text.splitlines()[1:] for text in self.app.requests
                if text.splitlines()[1].split()[0] in ('press', 'dpad', 'shot', 'wait', 'stick')]

    def test_a_batch_runs_once_and_marks_the_save_stale(self):
        self.lab.write_save(save_known(self))
        self.assertEqual(self.reader.save_resource()['status'], 'known')
        token = self.acquired()
        request = batch_request(token, ['shot before', 'press B 160 640', 'shot after'])
        record = self.cfr(request)
        self.assertEqual(record['status'], 'completed', record)
        self.assertEqual(record['result']['outcome'], 'done')
        self.assertEqual(len(record['result']['output']), 2)  # the two shot paths
        self.assertEqual(self.app.lines_run, ['shot before', 'press B 160 640', 'shot after'])
        save = self.reader.save_resource()
        self.assertEqual((save['status'], save['reason']), ('stale', 'cfr_batch'))
        self.assertEqual(self.cfr(request), record)  # an exact retry presses nothing again
        self.assertEqual(len(self.batches_sent()), 1)
        contracts.validate('request_record', record)

    def test_a_failing_line_stops_the_batch_and_is_named(self):
        token = self.acquired()
        self.app.line_fail = (1, 'timeout: home not seen within 3000 ms')
        record = self.cfr(batch_request(token, ['press HOME', 'expect home 3000', 'press A']))
        self.assertEqual(record['status'], 'completed')
        self.assertEqual((record['result']['outcome'], record['result']['failed_line']), ('line_failed', 2))
        self.assertEqual(self.app.lines_run, ['press HOME'])

    def test_refusals_send_nothing_and_leave_the_save_alone(self):
        def other_program(test):
            test.app.transition('start', 'lab', new_id())

        def ui_click(test):
            test.app.transition('stop', 'ui')

        def settings_page(test):
            test.lab.update_state(**{'panel.id': None, 'panel.kind': 'other', 'program.state': 'no_program'})

        def owner_claimed(test):
            test.lab.update_state(**{'channel.owner_claimed': True})

        cases = [
            dict(name='a program runs', input=(other_program, ['press A']), expectation='program_running'),
            dict(name='the user holds the console', input=(ui_click, ['press A']), expectation='console_held_by_user'),
            dict(name='the settings page', input=(settings_page, ['press A']), expectation='panel_not_program'),
            dict(name='a lab script owns the channel', input=(owner_claimed, ['press A']),
                 expectation='console_held_by_agent'),
            dict(name='a channel command', input=(None, ['press A', 'start']), expectation='invalid_arguments'),
            dict(name='close_game', input=(None, ['close_game']), expectation='invalid_arguments'),
            dict(name='over the time budget', input=(None, ['wait 30000', 'wait 30000']),
                 expectation='invalid_arguments'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.lab.write_save(save_known(self))
                token = self.acquired()
                setup, lines = case['input']
                if setup is not None:
                    setup(self)
                record = self.cfr(batch_request(token, lines))
                self.assertEqual((record['status'], record['refusal']['code']), ('refused', case['expectation']))
                self.assertEqual(self.app.lines_run, [])
                self.assertEqual(self.inbox(), [])
                self.assertIsNone(self.plane.save_mark('B'))

    def test_without_a_lease_nothing_is_pressed(self):
        record = self.cfr(batch_request('not-a-lease', ['press A']))
        self.assertEqual(record['refusal']['code'], 'lease_invalid')
        self.assertEqual(self.app.lines_run, [])

    def test_a_batch_the_app_never_took_is_withdrawn_and_the_save_stays_stale(self):
        """The mark is written before the request: a press may have happened if the daemon died after."""
        self.lab.write_save(save_known(self))
        token = self.acquired()
        self.app.serve = False
        record = self.cfr(batch_request(token, ['press A']))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'timeout'))
        self.assertEqual(self.inbox(), [])
        self.assertEqual(self.reader.save_resource()['reason'], 'cfr_batch')


if __name__ == '__main__':
    unittest.main()
