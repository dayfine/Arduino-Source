"""The core over an in-memory channel (`tests/fake_channel.py`): actions and queries judge typed
answers, with no `Lab/` folder at all.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import copy
import json
import unittest

from pa_lab import actions, app_query, lab_reader, timestamps
from pa_lab.channel import AppError, DescribeProgram, ErrorKind, GameOpen, Preflight, Start
from tests.control_fixtures import START, acquire, new_id, open_plane, temp_db_path
from tests.fake_channel import FakeChannel, journal
from tests.fake_lab import PRECHECKS, FakeClock, describe_reply, golden

CLIENT = 'claude:frlg-lane'
PANEL = 'PokemonFRLG:AutoStory'


def stopped_app():
    doc = copy.deepcopy(golden('app_state', 'stopped_ui'))
    doc['program']['last_transition'] = None
    doc['written_at'] = START
    doc['video']['captured_at'] = START
    return doc


PASSED = {'schema': 1, 'id': PANEL, 'verdict': 'passed', 'restored': True, 'failure': None, 'invalid': None,
          'checks': [{'name': check['name'], 'status': 'passed', 'message': ''} for check in PRECHECKS]}


def request(token, **arguments):
    return {'schema': 1, 'request_id': new_id(), 'console': 'B', 'lease_token': token,
            'arguments': dict({'panel': PANEL, 'options': {}}, **arguments)}


class CoreOverFakeChannelTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock(START)
        self.channel = FakeChannel(stopped_app(), self.clock)
        self.plane = open_plane(self, temp_db_path(self), self.clock)
        self.reader = lab_reader.ConsoleReader('B', self.channel, profile='lab-B', now=self.clock.now,
                                               pid_alive=lambda pid: True, control=self.plane)
        self.actions = actions.ProgramActions(self.plane, {'B': self.reader}, sleep=self.clock.sleep)

    def token(self):
        return self.plane.execute(CLIENT, acquire())['result']['lease_token']

    def run_request(self, token):
        """A run request with the token of a preflight that passed just now."""
        preflight = self.actions.preflight_program(CLIENT, request(token))
        self.assertEqual(preflight['status'], 'completed', preflight)
        return request(token, preflight_token=preflight['result']['preflight_token'])

    def answer(self, start):
        """The app answers describe_program and preflight as a program with prechecks, and `start` the start."""
        def answer_with(commands, request_id):
            if commands == (DescribeProgram(),):
                return journal(DescribeProgram, printed=[describe_reply(prechecks=PRECHECKS).strip()])
            if commands == (Preflight(options={}),):
                return journal(Preflight, printed=['preflight ' + json.dumps(PASSED)])
            return start(commands, request_id)
        self.channel.answer_with = answer_with

    def app_starts(self, commands, request_id):
        now = timestamps.format(self.clock.now())
        self.channel.update(**{'program.state': 'running', 'program.last_transition': {
            'action': 'start', 'source': 'lab', 'request_id': request_id, 'at': now}})
        return journal(Start)

    def test_run_program_sends_one_start_and_leases_the_run(self):
        self.answer(self.app_starts)
        record = self.actions.run_program(CLIENT, self.run_request(self.token()))
        self.assertEqual((record['status'], record['result']['correlated_by']), ('completed', 'request_id'))
        self.assertEqual(self.channel.sent[-1][2], (Start(controller_wait_s=10),))
        self.assertEqual(self.plane.lease('B')['state'], 'agent_running')

    def test_an_owner_claimed_answer_keeps_the_app_message_out_of_the_record(self):
        self.answer(lambda commands, request_id: journal(
            error=AppError(ErrorKind.OWNER_CLAIMED, 'owned by secret-owner-token')))
        record = self.actions.run_program(CLIENT, self.run_request(self.token()))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'console_held_by_agent'))
        self.assertNotIn('secret-owner-token', str(record))

    def test_a_program_state_answer_is_a_program_running_refusal(self):
        self.answer(lambda commands, request_id: journal(
            error=AppError(ErrorKind.PROGRAM_STATE, 'Program is RUNNING.', program_state='RUNNING')))
        record = self.actions.run_program(CLIENT, self.run_request(self.token()))
        self.assertEqual(record['refusal']['code'], 'program_running')

    def test_game_open_reads_the_printed_reply(self):
        self.channel.answer_with = lambda commands, request_id: journal(GameOpen, printed=['game_open no home'])
        result = app_query.game_open(self.reader, {'console': 'B'}, sleep=self.clock.sleep)
        self.assertEqual((result['open'], result['reason']), (False, 'home'))

    def test_an_unanswered_query_is_withdrawn_and_times_out(self):
        with self.assertRaises(app_query.Refused) as raised:
            app_query.game_open(self.reader, {'console': 'B'}, sleep=self.clock.sleep)
        self.assertEqual(raised.exception.refusal['code'], 'timeout')
        self.assertEqual(self.channel.inbox, set())


if __name__ == '__main__':
    unittest.main()
