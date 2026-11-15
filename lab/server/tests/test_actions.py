"""The first leased action, stop_program (Slice 5 of fork #131), against a fake app in a temp
`Lab/` folder: the ticket, the channel request it sends, and how the app's own report resolves
it, including the races the design names (duplicate requests, UI takeover, lease loss, app
restart, a terminal event before the response). No real profile or console is touched.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import sqlite3
import unittest

from pa_lab import actions, control, db, lab_reader, mcp_server, timestamps
from tests.control_fixtures import START, acquire, app_state, clock, new_id, open_plane, temp_db_path, transition
from tests.fake_lab import FakeApp, FakeLab

CLIENT = 'claude:frlg-lane'
OWNER = 'lab-B'
NEW_INSTANCE = '0b5e3f6c-9d2a-4e71-8c40-6f1a2b3c4d5e'


def stop_request(token, request_id=None):
    request = {'schema': 1, 'request_id': request_id or new_id(), 'tool': 'stop_program', 'console': 'B',
               'arguments': {}}
    if token is not None:
        request['lease_token'] = token
    return request


class ActionTest(unittest.TestCase):
    def setUp(self):
        self.clock = clock()
        self.path = temp_db_path(self)
        self.plane = open_plane(self, self.path, self.clock)
        self.lab = FakeLab(self)
        self.write_app(state='running', last=transition('start', 'lab', '2026-11-14T05:59:00.000Z', new_id()))
        self.app = FakeApp(self.lab, self.clock)
        self.reader = self.make_reader()

    def make_reader(self):
        return lab_reader.ConsoleReader('B', self.lab.dir, profile='lab-B', now=self.clock.now,
                                        pid_alive=lambda pid: True, control=self.plane)

    def write_app(self, state, last):
        doc = app_state(last, state)
        doc['written_at'] = START
        doc['video']['captured_at'] = START
        self.lab.write_state(doc)

    def acquired(self):
        record = self.plane.execute(CLIENT, acquire())
        self.assertEqual(record['status'], 'completed', record)
        return record['result']['lease_token']

    def stop(self, token, request_id=None):
        return actions.stop_program(self.plane, self.reader, CLIENT, stop_request(token, request_id), OWNER,
                                    sleep=self.clock.sleep)

    def tick(self):
        self.clock.sleep(0.5)
        self.reader.console_state()

    def status(self, request_id):
        return self.plane.request_status(CLIENT, 'B', request_id)

    def stop_requests(self):
        return [text for text in self.app.requests if text.rstrip().endswith('stop')]

    def events(self):
        return self.plane.next_events('reader', 'B', 0)['events']


class StopTests(ActionTest):
    def test_a_stop_is_pending_until_the_app_reports_it_stopped(self):
        token = self.acquired()
        request_id = new_id()
        record = self.stop(token, request_id)
        self.assertEqual(record['status'], 'pending')
        self.assertEqual(self.lab.state['program']['state'], 'stopping')
        self.tick()
        record = self.status(request_id)
        self.assertEqual(record['status'], 'completed')
        self.assertEqual((record['result']['outcome'], record['result']['source']), ('stopped', 'lab'))
        self.assertEqual(self.stop_requests(), ['owner lab-B\nrequest {}\nowner claim lab-B\nstop\n'.format(request_id)])
        last = self.events()[-1]
        self.assertEqual((last['type'], last['payload']['tool'], last['request_id']),
                         ('request_completed', 'stop_program', request_id))

    def test_the_stop_ends_a_running_lease(self):
        token = self.acquired()
        self.plane.begin_run('B', token, new_id(), self.lab.state['instance']['id'])
        self.stop(token)
        self.tick()
        lease = self.plane.lease('B')
        self.assertEqual(lease['state'], 'agent_idle')
        reasons = [e['payload'].get('reason') for e in self.events() if e['type'] == 'lease_changed']
        self.assertEqual(reasons[-1], 'run_ended')

    def test_a_terminal_event_before_the_response_is_the_response(self):
        self.app.stop_at_once = True
        record = self.stop(self.acquired())
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'stopped'))

    def test_an_exact_retry_sends_nothing_again(self):
        token = self.acquired()
        request_id = new_id()
        first = self.stop(token, request_id)
        again = self.stop(token, request_id)
        self.assertEqual(again['status'], first['status'])
        self.tick()
        self.assertEqual(self.stop(token, request_id), self.status(request_id))
        self.assertEqual(len(self.stop_requests()), 1)

    def test_a_retry_after_a_daemon_restart_gets_the_same_record(self):
        token = self.acquired()
        request_id = new_id()
        self.stop(token, request_id)
        self.plane = open_plane(self, self.path, self.clock)
        self.reader = self.make_reader()
        self.tick()  # the new daemon's first read resolves the ticket
        self.assertEqual(self.stop(token, request_id)['status'], 'completed')
        self.assertEqual(len(self.stop_requests()), 1)

    def test_nothing_running_completes_at_once_and_sends_nothing(self):
        self.lab.update_state(**{'program.state': 'stopped'})
        record = self.stop(self.acquired())
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'not_running'))
        self.assertEqual(self.stop_requests(), [])

    def test_refused_before_acting_sends_nothing(self):
        self.acquired()
        cases = [
            dict(name='no lease token', input=None, expectation='invalid_arguments'),
            dict(name='a token that does not hold the console', input='not-the-token', expectation='lease_invalid'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                if case['input'] is None:
                    with self.assertRaises(control.Refused) as caught:
                        self.stop(None)
                    self.assertEqual(caught.exception.refusal['code'], case['expectation'])
                else:
                    record = self.stop(case['input'])
                    self.assertEqual((record['status'], record['refusal']['code']), ('refused', case['expectation']))
        self.assertEqual(self.stop_requests(), [])

    def test_a_ui_click_before_the_call_takes_the_console(self):
        token = self.acquired()
        self.lab.update_state(**{'program.last_transition': transition('start', 'ui', START)})
        record = self.stop(token)
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'console_held_by_user'))
        self.assertEqual(self.stop_requests(), [])

    def test_an_expired_lease_is_refused(self):
        token = self.acquired()
        self.clock.t += 1201
        self.lab.update_state(written_at=timestamps.format(self.clock.now()))
        record = self.stop(token)
        self.assertEqual(record['refusal']['code'], 'lease_expired')

    def test_an_unavailable_app_is_refused(self):
        token = self.acquired()
        self.clock.t += 60  # no heartbeat for a minute
        record = self.stop(token)
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'app_unavailable'))


class RaceTests(ActionTest):
    def test_a_ui_stop_after_the_request_fails_the_ticket_and_hands_over(self):
        token = self.acquired()
        request_id = new_id()
        self.stop(token, request_id)
        self.lab.update_state(**{'program.last_transition': transition(
            'stop', 'ui', timestamps.format(self.clock.now()))})
        self.tick()
        record = self.status(request_id)
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'console_held_by_user'))
        self.assertEqual(self.plane.lease('B')['state'], 'user')

    def test_an_app_restart_before_the_stop_is_confirmed_fails_the_ticket(self):
        token = self.acquired()
        request_id = new_id()
        self.stop(token, request_id)
        self.lab.update_state(**{'instance.id': NEW_INSTANCE, 'program.state': 'stopped',
                                 'program.last_transition': None})
        self.tick()
        record = self.status(request_id)
        self.assertEqual((record['status'], record['refusal']['evidence']['reason']), ('failed', 'instance_replaced'))

    def test_a_run_that_ends_by_itself_completes_the_ticket(self):
        token = self.acquired()
        self.app.stop_silent = True  # the app took the request but has not answered yet
        request_id = new_id()
        self.assertEqual(self.stop(token, request_id)['status'], 'pending')
        self.lab.update_state(**{'program.state': 'stopped', 'program.last_transition': transition(
            'stop', 'program', timestamps.format(self.clock.now()))})
        self.tick()
        record = self.status(request_id)
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'ended'))

    def test_a_request_the_app_took_but_did_not_answer_stays_pending_until_reported(self):
        token = self.acquired()
        self.app.stop_silent = True
        request_id = new_id()
        self.assertEqual(self.stop(token, request_id)['status'], 'pending')
        self.lab.update_state(**{'program.state': 'stopped', 'program.last_transition': transition(
            'stop', 'lab', timestamps.format(self.clock.now()), request_id)})
        self.tick()
        self.assertEqual(self.status(request_id)['result']['outcome'], 'stopped')

    def test_a_busy_app_gets_the_request_withdrawn(self):
        self.app.serve = False
        record = self.stop(self.acquired())
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'timeout'))
        self.assertEqual(list((self.lab.dir / 'inbox').iterdir()), [])

    def test_a_channel_owned_by_another_script_fails_the_ticket(self):
        self.app.channel_owner = 'someone-else'
        record = self.stop(self.acquired())
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertIn('owned by someone-else', record['refusal']['message'])

    def test_a_run_that_ended_just_before_the_stop_line_is_its_end(self):
        token = self.acquired()
        original = self.app.stop

        def end_first(name, lines):
            self.lab.update_state(**{'program.state': 'stopped', 'program.last_transition': transition(
                'stop', 'program', timestamps.format(self.clock.now()))})
            original(name, lines)
        self.app.stop = end_first
        record = self.stop(token)
        self.assertEqual((record['status'], record['result']['outcome']), ('completed', 'ended'))
        self.assertEqual(record['result']['source'], 'program')


class McpTests(ActionTest):
    def setUp(self):
        super().setUp()
        self.mcp = mcp_server.LabMcp({'B': self.reader}, control_plane=self.plane, channel_owners={'B': OWNER},
                                     sleep=self.clock.sleep)

    def call(self, name, **arguments):
        return self.mcp.call('tools/call', {'name': name, 'arguments': dict(arguments, console='B')}, client=CLIENT)

    def test_stop_program_then_wait_for_its_event(self):
        token = self.acquired()
        request_id = new_id()
        reply = self.call('stop_program', request_id=request_id, lease_token=token)
        self.assertFalse(reply['isError'])
        self.assertEqual(reply['structuredContent']['status'], 'pending')
        head = self.plane.next_events('reader', 'B', 0)['next_after_seq']
        self.clock.sleep(0.5)  # the app reports stopped; the next read applies it
        page = self.call('next_events', after_seq=head)['structuredContent']
        self.assertEqual([(e['type'], e['request_id']) for e in page['events'] if e['type'].startswith('request_')],
                         [('request_completed', request_id)])
        self.assertEqual(self.call('request_status', request_id=request_id)['structuredContent']['status'],
                         'completed')

    def test_a_refusal_echoes_the_request_id(self):
        request_id = new_id()
        reply = self.call('stop_program', request_id=request_id, lease_token='nope')
        self.assertTrue(reply['isError'])
        self.assertEqual(reply['structuredContent']['request_id'], request_id)
        self.assertEqual(reply['structuredContent']['refusal']['code'], 'lease_invalid')

    def test_no_channel_owner_is_refused(self):
        self.mcp.channel_owners = {}
        reply = self.call('stop_program', request_id=new_id(), lease_token=self.acquired())
        self.assertEqual(reply['structuredContent']['refusal']['code'], 'app_unavailable')
        self.assertEqual(self.stop_requests(), [])


class MigrationTest(unittest.TestCase):
    def test_a_version_1_file_gains_the_ticket_instance_column(self):
        path = temp_db_path(self)
        path.parent.mkdir(parents=True)
        conn = sqlite3.connect(str(path))
        for statement in db.MIGRATIONS[0]:
            conn.execute(statement)
        conn.execute('PRAGMA user_version = 1')
        conn.commit()
        conn.close()
        database = db.Database(path)
        self.addCleanup(database.close)
        columns = [row[1] for row in database.read().execute('PRAGMA table_info(requests)')]
        self.assertIn('instance_id', columns)
        self.assertEqual(database.found_version, 1)


if __name__ == '__main__':
    unittest.main()
