"""The cfr_batch leased action (Slice 5b of fork #131) against a fake app in a temp `Lab/` folder:
the allowlist and state refusals, the request it sends, its journal as the result, the save fact
going stale, and the races (retry, daemon restart, busy app, app restart, foreign owner). No real
profile or console is touched.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import unittest

from pa_lab import actions, control, mcp_server, timestamps
from tests.control_fixtures import START, new_id, open_plane, transition
from tests.fake_lab import golden
from tests.test_actions import CLIENT, NEW_INSTANCE, OWNER, ActionTest

BATCH = ['shot before', 'press B 160 640', 'shot after']


def batch_request(token, lines=None, request_id=None):
    request = {'schema': 1, 'request_id': request_id or new_id(), 'tool': 'cfr_batch', 'console': 'B',
               'arguments': {'lines': list(BATCH if lines is None else lines)}}
    if token is not None:
        request['lease_token'] = token
    return request


class BatchTest(ActionTest):
    def setUp(self):
        super().setUp()
        self.lab.update_state(**{'program.state': 'stopped', 'program.last_transition': transition(
            'stop', 'lab', '2026-11-14T05:59:30.000Z', new_id())})

    def batch(self, token, lines=None, request_id=None):
        return actions.cfr_batch(self.plane, self.reader, CLIENT, batch_request(token, lines, request_id), OWNER,
                                 sleep=self.clock.sleep)

    def batch_requests(self):
        return [text for text in self.app.requests if 'owner claim' in text and not text.endswith('\nstop\n')]


class BatchTests(BatchTest):
    def test_a_batch_runs_and_returns_its_journal(self):
        token = self.acquired()
        request_id = new_id()
        record = self.batch(token, request_id=request_id)
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'done'))
        self.assertEqual(record['result']['journal'][-2:], ['ok 6 shot after @0-1ms', 'end'])
        self.assertEqual(self.batch_requests(), ['owner lab-B\nrequest {}\nowner claim lab-B\nshot before\n'
                                                 'press B 160 640\nshot after\n'.format(request_id)])
        last = self.events()[-1]
        self.assertEqual((last['type'], last['payload']['tool'], last['request_id']),
                         ('request_completed', 'cfr_batch', request_id))

    def test_a_failing_line_is_an_answer_with_its_line_number(self):
        self.app.batch_errors = {'press B 160 640': 'timeout'}
        record = self.batch(self.acquired())
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'line_failed'))
        self.assertEqual(record['result']['failed_line'], 2)
        self.assertEqual(record['result']['error'], 'error 5 (press B 160 640): timeout')

    def test_an_exact_retry_sends_nothing_again(self):
        token = self.acquired()
        request_id = new_id()
        first = self.batch(token, request_id=request_id)
        self.assertEqual(self.batch(token, request_id=request_id), first)
        self.assertEqual(len(self.batch_requests()), 1)

    def test_the_same_request_id_with_other_lines_is_refused(self):
        token = self.acquired()
        request_id = new_id()
        self.batch(token, request_id=request_id)
        with self.assertRaises(control.Refused) as caught:
            self.batch(token, ['press A'], request_id=request_id)
        self.assertEqual(caught.exception.refusal['code'], 'request_id_reused')


class RefusalTests(BatchTest):
    def test_refused_before_acting_sends_nothing_and_keeps_the_save(self):
        self.lab.write_save(golden('save_fact', 'frlg_checkpoint'))
        cases = [
            dict(name='a token that does not hold the console', input=dict(token='not-the-token'),
                 expectation='lease_invalid'),
            dict(name='a channel command', input=dict(lines=['press A', 'start']), expectation='invalid_arguments'),
            dict(name='over the time budget', input=dict(lines=['wait 30000', 'wait 30000']),
                 expectation='invalid_arguments'),
            dict(name='a running program', input=dict(state={'program.state': 'running'}),
                 expectation='program_running'),
            dict(name='a loading panel', input=dict(state={'program.state': 'not_ready'}),
                 expectation='program_not_stopped'),
            dict(name='the settings panel', input=dict(state={'program.state': 'no_program', 'panel.id': None,
                                                             'panel.kind': 'other'}),
                 expectation='panel_not_program'),
            dict(name='no controller', input=dict(state={'controller.state': 'not_connected'}),
                 expectation='controller_not_ready'),
            dict(name='stale video', input=dict(state={'video.state': 'stale'}, frames=False),
                 expectation='video_stale'),
        ]
        token = self.acquired()
        for case in cases:
            with self.subTest(case['name']):
                before = self.lab.state
                self.app.frames = case['input'].get('frames', True)
                self.lab.update_state(**case['input'].get('state', {}))
                record = self.batch(case['input'].get('token', token), case['input'].get('lines'))
                self.assertEqual((record['status'], record['refusal']['code']), ('refused', case['expectation']),
                                 record['refusal']['message'])
                self.lab.write_state(before)
                self.app.frames = True
        self.assertEqual(self.batch_requests(), [])
        self.assertEqual(self.reader.save_resource()['status'], 'known')

    def test_the_bad_line_is_named_in_the_refusal(self):
        record = self.batch(self.acquired(), ['press A', 'close_game'])
        self.assertEqual(record['refusal']['field'], 'arguments.lines[1]')

    def test_a_ui_click_before_the_call_takes_the_console(self):
        token = self.acquired()
        self.lab.update_state(**{'program.last_transition': transition('stop', 'ui', START)})
        record = self.batch(token)
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'console_held_by_user'))
        self.assertEqual(self.batch_requests(), [])

    def test_no_lease_token_cannot_be_keyed(self):
        with self.assertRaises(control.Refused) as caught:
            self.batch(None)
        self.assertEqual(caught.exception.refusal['code'], 'invalid_arguments')


class SaveTests(BatchTest):
    def test_a_sent_batch_marks_the_save_stale_until_a_newer_save(self):
        fact = golden('save_fact', 'frlg_checkpoint')
        fact['saved_at'] = '2026-11-14T05:00:00.000Z'
        self.lab.write_save(fact)
        self.assertEqual(self.reader.save_resource()['status'], 'known')
        self.batch(self.acquired())
        save = self.reader.save_resource()
        self.assertEqual((save['status'], save['reason']), ('stale', 'cfr_batch'))
        fact['saved_at'] = timestamps.format(self.clock.now() + 1)
        self.lab.write_save(fact)
        self.assertEqual(self.reader.save_resource()['status'], 'known')

    def test_a_batch_the_app_never_took_still_marks_the_save_stale(self):
        self.lab.write_save(golden('save_fact', 'frlg_checkpoint'))
        self.app.serve = False
        record = self.batch(self.acquired())
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'timeout'))
        self.assertEqual(self.reader.save_resource()['reason'], 'cfr_batch')

    def test_the_mark_survives_a_daemon_restart(self):
        self.lab.write_save(golden('save_fact', 'frlg_checkpoint'))
        self.batch(self.acquired())
        self.plane = open_plane(self, self.path, self.clock)
        self.reader = self.make_reader()
        self.assertEqual(self.reader.save_resource()['reason'], 'cfr_batch')


class RaceTests(BatchTest):
    def test_a_busy_app_gets_the_batch_withdrawn(self):
        self.app.serve = False
        record = self.batch(self.acquired())
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'timeout'))
        self.assertEqual(list((self.lab.dir / 'inbox').iterdir()), [])

    def test_a_batch_taken_but_never_answered_fails_with_timeout(self):
        self.app.batch_silent = True
        record = self.batch(self.acquired())
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'timeout'))

    def test_a_channel_owned_by_another_script_fails_and_nothing_runs(self):
        self.app.channel_owner = 'someone-else'
        record = self.batch(self.acquired())
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertIn('owned by someone-else', record['refusal']['message'])

    def test_a_program_started_before_the_batch_ran_fails_its_first_line(self):
        token = self.acquired()
        original = self.app.batch

        def started_first(name, lines):
            self.lab.update_state(**{'program.state': 'running'})
            original(name, lines)
        self.app.batch = started_first
        record = self.batch(token)
        self.assertEqual((record['result']['outcome'], record['result']['failed_line']), ('line_failed', 1))

    def test_a_retry_after_a_daemon_restart_collects_the_journal(self):
        token = self.acquired()
        request_id = new_id()
        stored = self.plane.begin_action(CLIENT, batch_request(token, request_id=request_id),
                                         self.lab.state['instance']['id'], lambda row: None)[0]
        self.assertEqual(stored['status'], 'pending')  # the daemon stopped after storing the ticket
        self.app.batch('palab_cfr_batch_' + request_id.replace('-', ''),
                       actions.request_text(OWNER, request_id, BATCH).splitlines())
        self.plane = open_plane(self, self.path, self.clock)
        self.reader = self.make_reader()
        record = self.batch(token, request_id=request_id)
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'done'))
        self.assertEqual(self.batch_requests(), [])

    def test_an_app_restart_fails_a_pending_batch(self):
        token = self.acquired()
        request_id = new_id()
        self.plane.begin_action(CLIENT, batch_request(token, request_id=request_id),
                                self.lab.state['instance']['id'], lambda row: None)
        self.lab.update_state(**{'instance.id': NEW_INSTANCE, 'program.last_transition': None})
        self.tick()
        record = self.status(request_id)
        self.assertEqual((record['status'], record['refusal']['evidence']['reason']), ('failed', 'instance_replaced'))


class McpTests(BatchTest):
    def setUp(self):
        super().setUp()
        self.mcp = mcp_server.LabMcp({'B': self.reader}, control_plane=self.plane, channel_owners={'B': OWNER},
                                     sleep=self.clock.sleep)

    def call(self, name, **arguments):
        return self.mcp.call('tools/call', {'name': name, 'arguments': dict(arguments, console='B')}, client=CLIENT)

    def test_the_tool_is_listed_with_its_lines(self):
        tools = {t['name']: t for t in self.mcp.call('tools/list', {})['tools']}
        self.assertEqual(tools['cfr_batch']['inputSchema']['required'], ['console', 'request_id', 'lease_token', 'lines'])

    def test_cfr_batch_returns_the_journal(self):
        reply = self.call('cfr_batch', request_id=new_id(), lease_token=self.acquired(), lines=BATCH)
        self.assertFalse(reply['isError'])
        self.assertEqual(reply['structuredContent']['result']['outcome'], 'done')

    def test_a_refusal_echoes_the_request_id_and_the_line(self):
        request_id = new_id()
        reply = self.call('cfr_batch', request_id=request_id, lease_token=self.acquired(), lines=['start'])
        self.assertTrue(reply['isError'])
        self.assertEqual(reply['structuredContent']['request_id'], request_id)
        self.assertEqual(reply['structuredContent']['refusal']['field'], 'arguments.lines[0]')

    def test_no_channel_owner_is_refused(self):
        self.mcp.channel_owners = {}
        reply = self.call('cfr_batch', request_id=new_id(), lease_token=self.acquired(), lines=BATCH)
        self.assertEqual(reply['structuredContent']['refusal']['code'], 'app_unavailable')
        self.assertEqual(self.batch_requests(), [])


if __name__ == '__main__':
    unittest.main()
