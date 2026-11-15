"""describe_program and game_open through the app's channel (Slice 4, fork #131), on a fake app.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import copy
import json
import unittest

from pa_lab import app_query, lab_reader, tools
from tests.fake_lab import PROGRAM_CONTRACT, FakeApp, FakeClock, FakeLab, golden

START = '2026-11-14T06:10:12.400Z'
INSTANCE = 'd8ea6a9d-1eab-4a40-b355-d88072b289a1'
NEW_INSTANCE = '11111111-2222-4333-8444-555555555555'


def describe_line(doc):
    return 'describe_program ' + json.dumps(doc, sort_keys=True, separators=(',', ':')) + '\n'


class AppQueryTest(unittest.TestCase):
    def setUp(self):
        self.lab = FakeLab(self)
        self.lab.write_state(golden('app_state', 'stopped_ui'))
        self.clock = FakeClock(START)
        self.app = FakeApp(self.lab, self.clock)
        self.console = lab_reader.ConsoleReader('B', self.lab.dir, profile='lab-B', now=self.clock.now,
                                                pid_alive=lambda pid: True)

    def ask(self, command, **arguments):
        query = getattr(app_query, command)
        return query(self.console, dict(arguments, console='B'), sleep=self.clock.sleep)

    def refusal(self, command, **arguments):
        with self.assertRaises(app_query.Refused) as raised:
            self.ask(command, **arguments)
        return raised.exception.refusal


class DescribeProgramTests(AppQueryTest):
    def test_returns_the_contract(self):
        result = self.ask('describe_program')
        self.assertEqual(result['program'], PROGRAM_CONTRACT)
        self.assertFalse(result['prechecks_supported'])
        self.assertEqual(result['instance_id'], INSTANCE)
        self.assertEqual(result['console'], 'B')

    def test_the_request_is_one_fixed_line(self):
        self.ask('describe_program')
        self.assertEqual(len(self.app.requests), 1)
        lines = self.app.requests[0].splitlines()
        self.assertRegex(lines[0], r'^request [0-9a-f-]{36}$')
        self.assertEqual(lines[1:], ['describe_program'])

    def test_a_precheck_list_is_supported(self):
        cases = [
            dict(name='two checks, in order',
                 input=[{'name': 'black_border', 'needs': 'No black border.', 'skippable': False},
                        {'name': 'free_party_slots', 'needs': '1 free party slot.', 'skippable': True}],
                 expectation=['black_border', 'free_party_slots']),
            dict(name='an empty list needs nothing', input=[], expectation=[]),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                doc = dict(PROGRAM_CONTRACT, prechecks=case['input'])
                self.app.replies['describe_program'] = describe_line(doc)
                result = self.ask('describe_program')
                self.assertTrue(result['prechecks_supported'])
                self.assertEqual([c['name'] for c in result['program']['prechecks']], case['expectation'])
                self.assertEqual(result['program']['prechecks'], case['input'])

    def test_a_bad_precheck_entry_is_refused(self):
        good = {'name': 'black_border', 'needs': 'No black border.', 'skippable': False}
        cases = [
            dict(name='not an object', input=['black_border'], expectation='prechecks'),
            dict(name='no name', input=[{k: v for k, v in good.items() if k != 'name'}], expectation='prechecks'),
            dict(name='empty name', input=[dict(good, name='')], expectation='prechecks'),
            dict(name='needs not a string', input=[dict(good, needs=None)], expectation='prechecks'),
            dict(name='skippable not a bool', input=[dict(good, skippable='no')], expectation='prechecks'),
            dict(name='a name twice', input=[good, dict(good, skippable=True)], expectation='prechecks'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.app.replies['describe_program'] = describe_line(dict(PROGRAM_CONTRACT, prechecks=case['input']))
                refusal = self.refusal('describe_program')
                self.assertEqual(refusal['code'], 'app_unavailable')
                self.assertIn(case['expectation'], refusal['message'])

    def test_refusals(self):
        bad_options = dict(PROGRAM_CONTRACT, options=['LANGUAGE'])
        no_id = {k: v for k, v in PROGRAM_CONTRACT.items() if k != 'id'}
        bad_prechecks = dict(PROGRAM_CONTRACT, prechecks='none')
        cases = [
            dict(name='not a program',
                 input=dict(error='describe_program: the selected panel is not a program.'),
                 expectation='panel_not_program'),
            dict(name='no options',
                 input=dict(error='describe_program: the selected program has no options the lab can read.'),
                 expectation='app_unavailable'),
            dict(name='schema 2', input=dict(reply=describe_line(dict(PROGRAM_CONTRACT, schema=2))),
                 expectation='unsupported_schema'),
            dict(name='bad json', input=dict(reply='describe_program {"id":\n'), expectation='app_unavailable'),
            dict(name='not an object', input=dict(reply='describe_program [1]\n'), expectation='app_unavailable'),
            dict(name='options not objects', input=dict(reply=describe_line(bad_options)),
                 expectation='app_unavailable'),
            dict(name='no id', input=dict(reply=describe_line(no_id)), expectation='app_unavailable'),
            dict(name='bad prechecks', input=dict(reply=describe_line(bad_prechecks)),
                 expectation='app_unavailable'),
            dict(name='no reply line', input=dict(reply=''), expectation='app_unavailable'),
            dict(name='two reply lines', input=dict(reply=describe_line(PROGRAM_CONTRACT) * 2),
                 expectation='app_unavailable'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                if 'error' in case['input']:
                    self.app.line_errors['describe_program'] = case['input']['error']
                else:
                    self.app.replies['describe_program'] = case['input']['reply']
                self.assertEqual(self.refusal('describe_program')['code'], case['expectation'])

    def test_closed_arguments(self):
        with self.assertRaises(tools.InvalidArguments) as raised:
            self.ask('describe_program', panel='PokemonFRLG:AutoStory')
        self.assertEqual(raised.exception.field, 'panel')
        self.assertEqual(self.app.requests, [])


class GameOpenTests(AppQueryTest):
    def test_yes(self):
        result = self.ask('game_open')
        self.assertEqual((result['open'], result['reason']), (True, None))
        self.assertEqual(self.app.requests[0].splitlines()[1:], ['game_open'])

    def test_no_with_each_reason(self):
        for reason in app_query.GAME_OPEN_REASONS:
            with self.subTest(reason):
                self.app.replies['game_open'] = 'game_open no {}\n'.format(reason)
                result = self.ask('game_open')
                self.assertEqual((result['open'], result['reason']), (False, reason))

    def test_refusals(self):
        cases = [
            dict(name='unknown reason', input=dict(reply='game_open no gremlins\n'), expectation='app_unavailable'),
            dict(name='no reason', input=dict(reply='game_open no\n'), expectation='app_unavailable'),
            dict(name='garbled', input=dict(reply='game_open maybe\n'), expectation='app_unavailable'),
            dict(name='line failed', input=dict(error='Unknown command: game_open (try "help")'),
                 expectation='app_unavailable'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                if 'error' in case['input']:
                    self.app.line_errors['game_open'] = case['input']['error']
                else:
                    self.app.replies['game_open'] = case['input']['reply']
                self.assertEqual(self.refusal('game_open')['code'], case['expectation'])


class SharedTests(AppQueryTest):
    def test_app_busy_times_out_and_withdraws(self):
        for command in ('describe_program', 'game_open'):
            with self.subTest(command):
                self.setUp()
                self.app.serve = False
                refusal = self.refusal(command)
                self.assertEqual(refusal['code'], 'timeout')
                self.assertEqual(list((self.lab.dir / 'inbox').iterdir()), [])

    def test_a_restart_during_the_call_is_refused(self):
        for command in ('describe_program', 'game_open'):
            with self.subTest(command):
                self.setUp()
                self.app.restart_on = command
                refusal = self.refusal(command)
                self.assertEqual(refusal['code'], 'app_unavailable')
                self.assertEqual(refusal['evidence']['reason'], 'instance_replaced')
                self.assertEqual(refusal['evidence']['new_instance_id'], NEW_INSTANCE)

    def test_app_down_is_refused_before_any_request(self):
        state = copy.deepcopy(self.lab.state)
        state['written_at'] = '2026-11-14T06:00:00.000Z'
        self.lab.write_state(state)
        self.assertEqual(self.refusal('game_open')['code'], 'app_unavailable')
        self.assertEqual(self.app.requests, [])


if __name__ == '__main__':
    unittest.main()
