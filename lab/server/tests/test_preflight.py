"""preflight_program and the token run_program needs (Slice 5b, fork #131), against the fake app's
proposed `preflight` command: options checked before anything is sent, the reply's verdicts, the
prior options restored (and a reply that says they were not), and every way a token goes stale.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import json
import unittest
from unittest import mock

from pa_lab import contracts, preflight
from tests.fake_lab import CURRENT_OPTIONS, describe_reply, golden
from tests.test_actions import CLIENT, NEW_INSTANCE, NO_TOKEN, ActionTest, preflight_request, run_request


class PreflightTest(ActionTest):
    def preflights_sent(self):
        """The `preflight <JSON>` lines the app took, as parsed candidates."""
        return [json.loads(text.splitlines()[-1].split(' ', 1)[1]) for text in self.app.requests
                if text.splitlines()[-1].startswith('preflight ')]

    def passed(self, token, options=None):
        record = self.preflight(preflight_request(token, options=options))
        self.assertEqual(record['status'], 'completed', record)
        return record


class OptionTests(PreflightTest):
    def test_bad_options_are_refused_before_anything_is_sent(self):
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
                token = self.acquired()
                record = self.preflight(preflight_request(token, options=case['input']))
                self.assertEqual((record['status'], record['refusal']['code'], record['refusal']['field']),
                                 ('refused', 'invalid_arguments', case['expectation']))
                self.assertEqual(self.preflights_sent(), [])
                self.assertEqual(self.app.options, CURRENT_OPTIONS)

    def test_good_options_reach_the_app_and_the_checks_see_them(self):
        token = self.acquired()
        options = {'START_SEGMENT': 3, 'NOTIFICATIONS': {'Enabled': False}}
        record = self.passed(token, options)
        self.assertEqual(self.preflights_sent(), [options])
        self.assertEqual(self.app.staged_seen, [{'LANGUAGE': 'en', 'START_SEGMENT': 3,
                                                 'NOTIFICATIONS': {'Enabled': False}}])
        self.assertEqual(self.app.options, CURRENT_OPTIONS)  # put back after the checks
        self.assertEqual([c['name'] for c in record['result']['checks']], ['game_open', 'free_party_slots'])
        self.assertEqual(record['result']['candidate_hash'], preflight.candidate_hash(options))
        # The app's hash of every option the program holds, as the checks left them (restored).
        self.assertEqual(record['result']['options_hash'], self.app.options_hash())
        contracts.validate('request_record', record)


class VerdictTests(PreflightTest):
    def test_a_failed_check_is_named_and_the_options_are_restored(self):
        token = self.acquired()
        self.app.check_outcomes['game_open'] = ('failed', 'The game is not open: the console shows HOME.',
                                                {'screen': 'home'})
        record = self.preflight(preflight_request(token, options={'START_SEGMENT': 7}))
        self.assertEqual(record['status'], 'failed')
        self.assertEqual(record['refusal'], {'code': 'precheck_failed', 'check': 'game_open',
                                             'message': 'The game is not open: the console shows HOME.',
                                             'evidence': {'screen': 'home'}})
        self.assertEqual(self.app.options, CURRENT_OPTIONS)
        self.assertEqual(self.plane.preflight(NO_TOKEN), None)
        self.assertIn(('request_failed', 'preflight_program'), self.event_types())

    def test_a_reply_that_did_not_restore_the_options_never_gives_a_token(self):
        """Even when every check passed: the panel now holds the candidate, not what the user set."""
        token = self.acquired()
        self.app.restore = False
        record = self.preflight(preflight_request(token, options={'START_SEGMENT': 7}))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertEqual(record['refusal']['evidence']['reason'], 'options_not_restored')
        self.assertIsNone(record['result'])

    def test_a_user_edit_during_the_checks_is_kept_and_named(self):
        """#601: the app does not overwrite an edit made in the panel during the checks."""
        token = self.acquired()
        self.app.user_edit = {'LANGUAGE': 'de'}
        record = self.preflight(preflight_request(token, options={'START_SEGMENT': 7}))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertEqual(record['refusal']['evidence']['edited_keys'], ['LANGUAGE'])
        self.assertEqual(self.app.options['LANGUAGE'], 'de')

    def test_other_verdicts(self):
        def unsupported(test):
            test.app.replies['describe_program'] = describe_reply(prechecks='unsupported')

        def app_refuses_combination(test):
            test.app.invalid = ('START_SEGMENT', 'segment 3 needs NEW_GAME off')

        def no_preflight_command(test):  # today's app
            test.app.line_errors['preflight'] = 'unknown command: preflight'

        def other_program_answers(test):
            test.app.preflight_id = 'PokemonBDSP:AutoStory'

        cases = [
            dict(name='the program declares no checks (nothing sent)', input=unsupported,
                 expectation=('refused', 'prechecks_unsupported', 0)),
            dict(name='check_validity refuses the combination', input=app_refuses_combination,
                 expectation=('failed', 'invalid_arguments', 1)),
            dict(name="today's app has no preflight command", input=no_preflight_command,
                 expectation=('failed', 'app_unavailable', 1)),
            dict(name='the reply names another program', input=other_program_answers,
                 expectation=('failed', 'app_unavailable', 1)),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                token = self.acquired()
                case['input'](self)
                record = self.preflight(preflight_request(token, options={'START_SEGMENT': 3}))
                self.assertEqual((record['status'], record['refusal']['code'], len(self.preflights_sent())),
                                 case['expectation'])

    def test_a_preflight_is_leased_and_retried_from_its_record(self):
        record = self.preflight(preflight_request('not-a-lease'))
        self.assertEqual(record['refusal']['code'], 'lease_invalid')
        token = self.acquired()
        request = preflight_request(token)
        first = self.preflight(request)
        again = self.preflight(request)
        self.assertEqual(first, again)
        self.assertEqual(len(self.preflights_sent()), 1)


class TokenTests(PreflightTest):
    def run_with(self, token, preflight_token, options=None):
        return self.run_program(run_request(token, options=options, preflight_token=preflight_token))

    def test_a_current_token_starts_the_run_with_its_options(self):
        token = self.acquired()
        options = {'START_SEGMENT': 3, 'NOTIFICATIONS': {'Enabled': False}}
        checked = self.passed(token, options)
        record = self.run_with(token, checked['result']['preflight_token'], options)
        self.assertEqual(record['status'], 'completed', record)
        start = [t for t in self.app.requests if t.splitlines()[-1].startswith('start')][0].splitlines()
        # In key order: the record stores the options as canonical JSON.
        self.assertEqual(start[1:], ['set NOTIFICATIONS.Enabled false', 'set START_SEGMENT 3', 'start 10'])
        self.assertEqual(self.app.options['START_SEGMENT'], 3)

    def test_stale_tokens_are_refused_and_nothing_is_sent(self):
        def run_and_end(test):
            test.app.transition('start', 'lab', 'aaaaaaaa-1111-4222-8333-444444444444')
            test.app.transition('stop', 'program')

        def app_restart(test):
            test.lab.update_state(**{'instance.id': NEW_INSTANCE})

        def save_written(test):
            test.clock.sleep(1)
            fact = save_known(test)
            fact['saved_at'] = test.lab.state['written_at']
            test.lab.write_save(fact)

        def cfr_batch(test):
            record = test.actions.cfr_batch(CLIENT, {'schema': 1, 'request_id': '0f0f0f0f-1111-4222-8333-444444444444',
                                                     'console': 'B', 'lease_token': test.token,
                                                     'arguments': {'lines': ['press B']}})
            test.assertEqual(record['status'], 'completed', record)

        def user_edits_another_option(test):
            # In the panel, an option the candidate does not name: no transition, no save, same instance.
            test.app.options['LANGUAGE'] = 'de'
            test.clock.sleep(1)  # the app's next state.json write

        def too_old(test):
            test.clock.sleep(preflight.TOKEN_TTL_S + 1)

        cases = [
            dict(name='the program ran and ended', input=(run_and_end, None), expectation=['transition']),
            dict(name='the app restarted', input=(app_restart, None), expectation=['instance_id']),
            dict(name='a save was written', input=(save_written, None), expectation=['save']),
            dict(name='a cfr_batch ran', input=(cfr_batch, None), expectation=['save']),
            dict(name='a cfr_batch ran with no save fact', input=(cfr_batch, None, None, 'no fact'),
                 expectation=['save']),
            dict(name='other options', input=(None, {'START_SEGMENT': 4}), expectation=['candidate_hash']),
            dict(name='an option the candidate did not name was edited', input=(user_edits_another_option, None),
                 expectation=['options_hash']),
            dict(name='too old', input=(too_old, None), expectation='expired'),
            dict(name='never issued', input=(None, None, NO_TOKEN), expectation='unknown_token'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.token = self.acquired()
                if 'no fact' not in case['input']:
                    self.lab.write_save(save_known(self))
                checked = self.passed(self.token, {'START_SEGMENT': 3})
                setup, options = case['input'][:2]
                if setup is not None:
                    setup(self)
                run_token = case['input'][2] if len(case['input']) > 2 and case['input'][2] else \
                    checked['result']['preflight_token']
                record = self.run_with(self.token, run_token, options or {'START_SEGMENT': 3})
                self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'preflight_stale'),
                                 record)
                evidence = record['refusal']['evidence']
                if isinstance(case['expectation'], list):
                    self.assertEqual(evidence['changed'], case['expectation'])
                else:
                    self.assertEqual(evidence['reason'], case['expectation'])
                self.assertEqual(self.sent('start'), [])

    def test_an_app_that_reports_no_options_hash_still_runs(self):
        """Before the app writes program.options_hash (#601), the token binds the candidate only."""
        self.app.hash_options = False
        token = self.acquired()
        checked = self.passed(token, {'START_SEGMENT': 3})
        self.assertIsNone(checked['result']['options_hash'])
        record = self.run_with(token, checked['result']['preflight_token'], {'START_SEGMENT': 3})
        self.assertEqual(record['status'], 'completed', record)

    def test_a_token_is_bound_to_its_lease(self):
        token = self.acquired()
        checked = self.passed(token)
        self.plane.execute(CLIENT, {'schema': 1, 'request_id': '1f1f1f1f-1111-4222-8333-444444444444',
                                    'tool': 'release_console', 'console': 'B', 'lease_token': token,
                                    'arguments': {}})
        other = self.acquired()
        record = self.run_with(other, checked['result']['preflight_token'])
        self.assertEqual(record['refusal']['evidence']['changed'], ['lease'])

    def test_a_token_runs_once(self):
        """The run's own start changes the last transition, so the same token cannot start a second run."""
        token = self.acquired()
        preflight_token = self.passed(token)['result']['preflight_token']
        self.assertEqual(self.run_with(token, preflight_token)['status'], 'completed')
        self.app.transition('stop', 'program')
        again = self.run_with(token, preflight_token)
        self.assertEqual((again['refusal']['code'], again['refusal']['evidence']['changed']),
                         ('preflight_stale', ['transition']))
        self.assertEqual(len(self.sent('start')), 1)


class PendingPreflightTests(PreflightTest):
    """A preflight the app took and has not answered: what ends it, and that it never wedges the console."""

    def held(self, during=None):
        """A preflight the app takes and never answers; `during` runs at the settle loop's first sleep."""
        self.app.hold.add('preflight')
        if during is not None:
            self.app.after['preflight'] = during
        self.token = self.acquired()
        return self.preflight(preflight_request(self.token, options={'START_SEGMENT': 3}))

    def test_a_ui_start_during_a_preflight_hands_the_console_to_the_user(self):
        record = self.held(lambda app: app.transition('start', 'ui'))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'console_held_by_user'))
        self.assertIsNone(record['result'])

    def test_an_app_restart_during_a_preflight_ends_it(self):
        record = self.held(lambda app: app.lab.update_state(**{'instance.id': NEW_INSTANCE}))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertEqual(record['refusal']['evidence']['reason'], 'instance_replaced')

    def test_a_reply_already_there_at_the_first_poll_is_read(self):
        """The app answers before the daemon's first look: no sleep, the token is issued at once."""
        token = self.acquired()
        send = self.reader.channel.send

        def send_and_answer(*args):
            send(*args)
            self.app.tick()
        with mock.patch.object(self.reader.channel, 'send', side_effect=send_and_answer):
            before = self.clock.now()
            record = self.preflight(preflight_request(token, options={'START_SEGMENT': 3}))
        self.assertEqual(record['status'], 'completed', record)
        self.assertEqual(self.clock.now(), before)

    def test_a_preflight_never_answered_fails_after_its_wait_and_frees_the_console(self):
        record = self.held()
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'timeout'))
        self.assertEqual(record['refusal']['evidence']['reason'], 'outcome_unknown')
        self.app.hold.clear()
        self.passed(self.token)


def save_known(test):
    """A save fact from before the test, so console://B/save is `known`."""
    fact = golden('save_fact', 'frlg_checkpoint')
    fact['saved_at'] = '2026-11-14T05:00:00.000Z'
    return fact


if __name__ == '__main__':
    unittest.main()
