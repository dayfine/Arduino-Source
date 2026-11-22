"""run_program's server preflight (the preflight split, lab/server/README.md) against the fake app:
describe_program read first, the program's start-check list, the candidate options against its
static contract, options that would turn a start check off, the `set` lines and `start` in one
request, and a start check's failure reported as the run's outcome (`program_failed`).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import copy
import unittest

from pa_lab import contracts, control, preflight, timestamps
from tests.fake_lab import CURRENT_OPTIONS, PROGRAM_CONTRACT, describe_reply, golden
from tests.test_actions import ActionTest, run_request

# FRLG RNG's free party slots setting: a boolean that turns the start check off when true.
SKIPPER = {'key': 'SKIP_FREE_PARTY_SLOTS', 'type': 'boolean', 'default': False, 'skips_check': 'free_party_slots'}


class ServerPreflightTest(ActionTest):
    def starts_sent(self):
        """The start requests the app took, each as its command lines (without `request <id>`)."""
        return [text.splitlines()[1:] for text in self.app.requests if text.splitlines()[-1].startswith('start')]

    def describes_sent(self):
        return [text for text in self.app.requests if text.splitlines()[-1] == 'describe_program']


class OptionTests(ServerPreflightTest):
    def test_bad_options_are_refused_before_anything_is_set(self):
        cases = [
            dict(name='unknown key', input={'START_CHECKPOINT': '13.01'}, expectation='options.START_CHECKPOINT'),
            dict(name='disabled enum entry', input={'LANGUAGE': 'ja'}, expectation='options.LANGUAGE'),
            dict(name='enum slug not listed', input={'LANGUAGE': 'fr'}, expectation='options.LANGUAGE'),
            dict(name='integer above max', input={'START_SEGMENT': 41}, expectation='options.START_SEGMENT'),
            dict(name='integer given as a string', input={'START_SEGMENT': '3'}, expectation='options.START_SEGMENT'),
            dict(name='boolean given as an integer', input={'START_SEGMENT': True}, expectation='options.START_SEGMENT'),
            dict(name='group given a scalar', input={'NOTIFICATIONS': False}, expectation='options.NOTIFICATIONS'),
            dict(name='unknown key in a group', input={'NOTIFICATIONS': {'Email': True}},
                 expectation='options.NOTIFICATIONS.Email'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                record = self.run_program(run_request(self.acquired(), options=case['input']))
                self.assertEqual((record['status'], record['refusal']['code'], record['refusal']['field']),
                                 ('refused', 'invalid_arguments', case['expectation']))
                self.assertEqual(self.starts_sent(), [])
                self.assertEqual(self.app.options, CURRENT_OPTIONS)

    def test_good_options_are_set_and_started_in_one_request(self):
        options = {'START_SEGMENT': 3, 'NOTIFICATIONS': {'Enabled': False}}
        record = self.run_program(run_request(self.acquired(), options=options))
        self.assertEqual(record['status'], 'completed', record)
        # In key order: the record stores the options as canonical JSON.
        self.assertEqual(self.starts_sent(), [['set NOTIFICATIONS.Enabled false', 'set START_SEGMENT 3', 'start 10']])
        self.assertEqual(self.app.options, {'LANGUAGE': 'en', 'START_SEGMENT': 3, 'NOTIFICATIONS': {'Enabled': False}})
        contracts.validate('request_record', record)

    def test_an_option_that_turns_a_start_check_off_must_be_given_as_false(self):
        self.app.replies['describe_program'] = describe_reply(
            options=PROGRAM_CONTRACT['options'] + [SKIPPER], prechecks=[
                {'name': 'free_party_slots', 'needs': '1 free party slot.', 'skippable': True}])
        cases = [
            dict(name='set true', input={'SKIP_FREE_PARTY_SLOTS': True}, expectation='refused'),
            dict(name='left out (the panel may hold true)', input={}, expectation='refused'),
            dict(name='set false', input={'SKIP_FREE_PARTY_SLOTS': False}, expectation='completed'),
        ]
        token = self.acquired()
        for case in cases:  # in this order: only the last one starts
            with self.subTest(case['name']):
                record = self.run_program(run_request(token, options=case['input']))
                self.assertEqual(record['status'], case['expectation'], record)
                if case['expectation'] == 'refused':
                    self.assertEqual((record['refusal']['code'], record['refusal']['field']),
                                     ('invalid_arguments', 'options.SKIP_FREE_PARTY_SLOTS'))
        self.assertEqual(self.starts_sent(), [['set SKIP_FREE_PARTY_SLOTS false', 'start 10']])

    def test_a_failed_set_line_names_the_options_the_panel_keeps(self):
        """The app has no undo: the set lines before the failing one stay applied, and the refusal says so."""
        options = {'START_SEGMENT': 3, 'NOTIFICATIONS': {'Enabled': False}}
        self.app.set_fail = ('START_SEGMENT', 'Unknown option: START_SEGMENT')
        record = self.run_program(run_request(self.acquired(), options=options))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'), record)
        evidence = record['refusal']['evidence']
        self.assertEqual((evidence['applied_options'], evidence['failed_option']),
                         ({'NOTIFICATIONS.Enabled': False}, 'START_SEGMENT'))
        self.assertIn('NOTIFICATIONS.Enabled', record['refusal']['message'])
        self.assertEqual(self.app.lab.state['program']['state'], 'stopped')

    def test_a_failed_first_set_line_applies_nothing(self):
        self.app.set_fail = ('NOTIFICATIONS.Enabled', 'Unknown option: NOTIFICATIONS.Enabled')
        record = self.run_program(run_request(self.acquired(), options={'START_SEGMENT': 3,
                                                                          'NOTIFICATIONS': {'Enabled': False}}))
        self.assertEqual(record['refusal']['code'], 'app_unavailable', record)
        self.assertNotIn('applied_options', record['refusal']['evidence'])
        self.assertEqual(self.app.options, CURRENT_OPTIONS)


class DescribeTests(ServerPreflightTest):
    def test_the_program_must_report_start_checks(self):
        self.app.replies['describe_program'] = describe_reply(prechecks='unsupported')
        record = self.run_program(run_request(self.acquired()))
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'prechecks_unsupported'))
        self.assertEqual(self.starts_sent(), [])
        self.assertIn(('request_failed', 'run_program'), self.event_types())

    def test_describe_program_refusals_are_stored_after_the_lease(self):
        def other_program(test):
            test.app.replies['describe_program'] = describe_reply(id='PokemonBDSP:AutoStory')

        def line_fails(test):
            test.app.line_errors['describe_program'] = 'boom'

        def restart_during_describe(test):
            test.app.restart_on = 'describe_program'

        cases = [
            dict(name='the app describes another program', input=other_program,
                 expectation=('invalid_arguments', None)),
            dict(name='the describe_program line fails', input=line_fails, expectation=('app_unavailable', None)),
            dict(name='the app restarts during describe_program', input=restart_during_describe,
                 expectation=('app_unavailable', 'instance_replaced')),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                token = self.acquired()
                case['input'](self)
                record = self.run_program(run_request(token))
                self.assertEqual(record['status'], 'refused', record)
                self.assertEqual((record['refusal']['code'], (record['refusal'].get('evidence') or {}).get('reason')),
                                 case['expectation'])
                self.assertEqual(self.starts_sent(), [])

    def test_the_lease_is_checked_before_a_describe_refusal(self):
        self.app.line_errors['describe_program'] = 'boom'
        record = self.run_program(run_request('not-a-lease'))
        self.assertEqual(record['refusal']['code'], 'lease_invalid')

    def test_no_describe_is_sent_when_the_console_cannot_run(self):
        token = self.acquired()
        self.lab.update_state(**{'controller.state': 'not_connected'})
        record = self.run_program(run_request(token))
        self.assertEqual(record['refusal']['code'], 'controller_not_ready')
        self.assertEqual(self.describes_sent(), [])

    def test_an_exact_retry_reads_nothing_and_sends_nothing(self):
        request = run_request(self.acquired(), options={'START_SEGMENT': 3})
        first = self.run_program(request)
        again = self.run_program(request)
        self.assertEqual(first, again)
        self.assertEqual((len(self.describes_sent()), len(self.starts_sent())), (1, 1))

    def test_there_is_no_preflight_program_tool(self):
        self.assertFalse(hasattr(self.actions, 'preflight_program'))
        self.assertNotIn('preflight_program', contracts.TOOL_ARGUMENTS)
        self.assertNotIn('preflight_token', contracts.TOOL_ARGUMENTS['run_program'].fields)


class SaveHookTests(unittest.TestCase):
    def test_the_save_check_passes_until_save_facts_exist(self):
        """Slice 6d replaces this with save_mismatch; until then nothing is refused for the save."""
        self.assertIsNone(preflight.check_save({'status': 'unknown', 'reason': 'no_fact', 'since': None},
                                               dict(PROGRAM_CONTRACT), {}))


class StartCheckOutcomeTests(ServerPreflightTest):
    """A start check runs after the start: its failure is the run's outcome, not a refusal."""

    def end_with_outcome(self, request_id, outcome):
        """The program stops itself and writes its outcome in the same state.json, as the app does."""
        at = timestamps.format(self.clock.now())
        self.lab.update_state(**{
            'program.state': 'stopped', 'written_at': at,
            'program.last_transition': {'action': 'stop', 'source': 'program', 'request_id': None, 'at': at},
            'program.last_outcome': dict(outcome, at=at, request_id=request_id)})

    def failure(self):
        return {'run_id': None, 'outcome': 'start_check_failed', 'check': 'free_party_slots',
                'message': 'Free party slots: needs 1 empty slot, found 0.', 'evidence': {'needed': 1, 'found': 0}}

    def program_failed(self):
        return [e for e in self.plane.next_events('reader', 'B', 0, limit=500)['events']
                if e['type'] == 'program_failed']

    def test_a_failed_start_check_is_a_program_failed_event_for_the_run(self):
        request = run_request(self.acquired())
        record = self.run_program(request)
        self.assertEqual(record['status'], 'completed', record)
        self.end_with_outcome(request['request_id'], self.failure())
        self.reader.console_state()
        self.reader.console_state()  # observed once
        events = self.program_failed()
        self.assertEqual(len(events), 1)
        event = events[0]
        self.assertEqual(event['request_id'], request['request_id'])
        self.assertEqual(event['payload'], {'run_id': None, 'verdict': 'start_check_failed', 'checkpoint': None,
                                            'check': 'free_party_slots',
                                            'message': 'Free party slots: needs 1 empty slot, found 0.',
                                            'evidence': {'needed': 1, 'found': 0}})
        contracts.validate('event', event)
        self.assertEqual(self.lease(), 'agent_idle')

    def test_a_stop_without_an_outcome_is_no_program_failed(self):
        self.run_program(run_request(self.acquired()))
        self.app.transition('stop', 'program')
        self.reader.console_state()
        self.assertEqual(self.program_failed(), [])

    def test_the_golden_app_state_gives_the_golden_event_payload(self):
        app = golden('app_state', 'start_check_failed')
        want = golden('event', 'program_failed_start_check')
        got = control.terminal_event(copy.deepcopy(app))
        self.assertEqual((got['payload'], got['request_id']), (want['payload'], want['request_id']))


if __name__ == '__main__':
    unittest.main()
