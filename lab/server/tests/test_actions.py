"""run_program and stop_program against the fake app (Slice 5a, fork #131): the refusals, an exact
retry, and the races of the design's Slice 5 acceptance that need no console: a duplicate request,
a UI takeover during start and stop, lease loss, an app restart between requests, and a terminal
event before the response. A FakeClock drives the app; nothing touches a real profile.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import copy
import unittest
from unittest import mock

from pa_lab import actions, contracts, control, lab_reader, mcp_server
from tests.control_fixtures import START, acquire, new_id, open_plane, release, temp_db_path
from tests.fake_lab import PRECHECKS, FakeApp, FakeClock, FakeLab, describe_reply, golden

PANEL = 'PokemonFRLG:AutoStory'
CLIENT = 'claude:frlg-lane'
NEW_INSTANCE = '0b5e3f6c-9d2a-4e71-8c40-6f1a2b3c4d5e'


def stopped_app():
    doc = copy.deepcopy(golden('app_state', 'stopped_ui'))
    doc['program']['last_transition'] = None
    doc['written_at'] = START
    doc['video']['captured_at'] = START
    return doc


NO_TOKEN = 'pf1.never-issued'


def run_request(token, request_id=None, panel=PANEL, options=None, **arguments):
    return {'schema': 1, 'request_id': request_id or new_id(), 'console': 'B', 'lease_token': token,
            'arguments': dict({'panel': panel, 'options': {} if options is None else options}, **arguments)}


def preflight_request(token, request_id=None, panel=PANEL, options=None):
    return {'schema': 1, 'request_id': request_id or new_id(), 'console': 'B', 'lease_token': token,
            'arguments': {'panel': panel, 'options': {} if options is None else options}}


def stop_request(token, request_id=None):
    return {'schema': 1, 'request_id': request_id or new_id(), 'console': 'B', 'lease_token': token,
            'arguments': {}}


class ActionTest(unittest.TestCase):
    def setUp(self):
        self.clock = FakeClock(START)
        self.lab = FakeLab(self)
        self.lab.write_state(stopped_app())
        self.app = FakeApp(self.lab, self.clock)
        self.app.replies['describe_program'] = describe_reply(prechecks=PRECHECKS)
        self.alive = True
        self.path = temp_db_path(self)
        self.open_daemon()

    def open_daemon(self):
        """A (re)started daemon: a new control plane, reader and actions on the same SQLite file."""
        self.plane = open_plane(self, self.path, self.clock)
        self.reader = lab_reader.ConsoleReader('B', self.lab.dir, profile='lab-B', now=self.clock.now,
                                               pid_alive=lambda pid: self.alive, control=self.plane)
        self.actions = actions.ProgramActions(self.plane, {'B': self.reader}, sleep=self.clock.sleep)

    def acquired(self):
        record = self.plane.execute(CLIENT, acquire())
        self.assertEqual(record['status'], 'completed', record)
        return record['result']['lease_token']

    def run_program(self, request):
        return self.actions.run_program(CLIENT, request)

    def preflight(self, request):
        return self.actions.preflight_program(CLIENT, request)

    def run_req(self, token, request_id=None, panel=PANEL, options=None, **arguments):
        """A run request with a token from a preflight of the same panel and options made now; when
        that preflight does not pass, a token never issued (for refusals checked before the token)."""
        if 'preflight_token' not in arguments:
            try:
                record = self.preflight(preflight_request(token, panel=panel, options=options))
            except control.Refused:
                record = {'status': 'refused'}
            passed = record['status'] == 'completed'
            arguments['preflight_token'] = record['result']['preflight_token'] if passed else NO_TOKEN
        return run_request(token, request_id, panel, options, **arguments)

    def stop_program(self, request):
        return self.actions.stop_program(CLIENT, request)

    def started(self, token):
        record = self.run_program(self.run_req(token))
        self.assertEqual(record['status'], 'completed', record)
        return record

    def sent(self, command=None):
        """The start/stop requests the app took: [(request_id, command line)]."""
        out = []
        for text in self.app.requests:
            lines = text.splitlines()
            if lines[-1].split()[0] in ('start', 'stop') and (command is None or lines[-1].startswith(command)):
                out.append((lines[0].split()[1], lines[-1]))
        return out

    def inbox(self):
        return sorted(p.name for p in (self.lab.dir / 'inbox').iterdir())

    def lease(self):
        return self.plane.lease('B')['state']

    def event_types(self):
        return [(e['type'], e['payload'].get('reason') or e['payload'].get('tool'))
                for e in self.plane.next_events('reader', 'B', 0, limit=500)['events']]

    def on_first_sleep(self, hook):
        """Runs `hook` once, at the first sleep after this call, before the app's own tick."""
        def once():
            self.clock.on_sleep.remove(once)
            hook()
        self.clock.on_sleep.insert(0, once)


class RunTests(ActionTest):
    def test_a_run_starts_once_and_the_lease_runs(self):
        token = self.acquired()
        request = self.run_req(token)
        record = self.run_program(request)
        self.assertEqual(record['status'], 'completed')
        self.assertEqual(record['arguments'], {'panel': PANEL, 'options': {},
                                               'preflight_token': request['arguments']['preflight_token']})
        self.assertEqual(record['result']['correlated_by'], 'request_id')
        self.assertIsNone(record['result']['ended'])
        self.assertEqual(self.sent(), [(request['request_id'], 'start 10')])
        self.assertEqual(self.lease(), 'agent_running')
        self.assertEqual(self.lab.state['program']['last_transition']['request_id'], request['request_id'])
        self.assertIn(('request_completed', 'run_program'), self.event_types())
        self.assertIn(('lease_changed', 'run_started'), self.event_types())
        contracts.validate('request_record', record)

    def test_an_exact_retry_returns_the_stored_record_and_sends_nothing(self):
        token = self.acquired()
        request = self.run_req(token)
        first = self.run_program(request)
        self.clock.sleep(5)
        again = self.run_program(request)
        self.assertEqual(first, again)
        self.assertEqual(len(self.sent()), 1)

    def test_a_retry_of_a_refusal_returns_the_same_refusal(self):
        token = self.acquired()
        request = self.run_req(token, panel='PokemonBDSP:AutoStory')
        first = self.run_program(request)
        self.lab.update_state(**{'panel.id': 'PokemonBDSP:AutoStory'})
        again = self.run_program(request)
        self.assertEqual((first['status'], first['refusal']['code']), ('refused', 'invalid_arguments'))
        self.assertEqual(first, again)
        self.assertEqual(self.sent(), [])

    def test_a_duplicate_while_the_first_is_in_flight_gets_the_pending_record(self):
        token = self.acquired()
        request = self.run_req(token)
        seen = {}
        self.on_first_sleep(lambda: seen.update(
            duplicate=self.run_program(request),
            other=self.run_program(run_request(token, preflight_token=NO_TOKEN))))
        record = self.run_program(request)
        self.assertEqual(seen['duplicate']['status'], 'pending')
        self.assertEqual((seen['other']['status'], seen['other']['refusal']['code']),
                         ('refused', 'program_not_stopped'))
        self.assertEqual(record['status'], 'completed')
        self.assertEqual(len(self.sent()), 1)

    def test_the_same_request_id_with_other_arguments_is_refused_unstored(self):
        token = self.acquired()
        request_id = new_id()
        self.run_program(self.run_req(token, request_id))
        with self.assertRaises(control.Refused) as raised:
            self.run_program(self.run_req(token, request_id, panel='PokemonBDSP:AutoStory'))
        self.assertEqual(raised.exception.refusal['code'], 'request_id_reused')

    def test_refusals_send_nothing_and_are_stored(self):
        def ui_click(test):
            test.app.transition('stop', 'ui')

        def expire(test):
            test.clock.sleep(1200)

        def other_program(test):
            test.app.transition('start', 'lab', new_id())

        def app_gone(test):
            test.alive = False

        def other_panel(test):
            test.lab.update_state(**{'panel.id': 'PokemonBDSP:AutoStory'})

        def settings_page(test):
            test.lab.update_state(**{'panel.id': None, 'panel.kind': 'other', 'program.state': 'no_program'})

        def controller(test):
            test.lab.update_state(**{'controller.state': 'not_connected'})

        def owner_claimed(test):
            test.lab.update_state(**{'channel.owner_claimed': True})

        cases = [
            dict(name='the user holds the lease', input=(ui_click, {}), expectation='console_held_by_user'),
            dict(name='no lease', input=(None, {'lease_token': 'not-a-lease'}), expectation='lease_invalid'),
            dict(name='the lease expired', input=(expire, {}), expectation='lease_expired'),
            dict(name='another program runs', input=(other_program, {}), expectation='program_running'),
            dict(name='the app is not running', input=(app_gone, {}), expectation='app_unavailable'),
            dict(name='another panel is selected', input=(other_panel, {}), expectation='invalid_arguments'),
            dict(name='the settings page is selected', input=(settings_page, {}), expectation='panel_not_program'),
            dict(name='the controller is not ready', input=(controller, {}), expectation='controller_not_ready'),
            dict(name='a lab script owns the channel', input=(owner_claimed, {}), expectation='console_held_by_agent'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                token = self.acquired()
                setup, changes = case['input']
                if setup is not None:
                    setup(self)
                request = run_request(changes.pop('lease_token', token), preflight_token=NO_TOKEN, **changes)
                record = self.run_program(request)
                self.assertEqual((record['status'], record['refusal']['code']), ('refused', case['expectation']))
                self.assertEqual(self.run_program(request), record)
                self.assertEqual(self.sent(), [])
                self.assertEqual(self.inbox(), [])
                self.assertIn(('request_failed', 'run_program'), self.event_types())


class StopTests(ActionTest):
    def test_a_stop_completes_when_the_program_is_stopped(self):
        token = self.acquired()
        self.started(token)
        self.app.stop_ticks = 3
        request = stop_request(token)
        record = self.stop_program(request)
        self.assertEqual(record['status'], 'completed')
        self.assertEqual(record['result']['stopped_by'], 'lab')
        self.assertEqual(record['result']['correlated_by'], 'request_id')
        self.assertEqual(self.lab.state['program']['state'], 'stopped')
        self.assertEqual(self.sent('stop'), [(request['request_id'], 'stop')])
        self.assertEqual(self.lease(), 'agent_idle')
        self.assertIn(('lease_changed', 'run_ended'), self.event_types())

    def test_nothing_running_is_answered_without_sending(self):
        token = self.acquired()
        record = self.stop_program(stop_request(token))
        self.assertEqual(record['status'], 'completed')
        self.assertEqual((record['result']['sent'], record['result']['state']), (False, 'stopped'))
        self.assertEqual(self.sent(), [])

    def test_refusals_send_nothing(self):
        def ui_click(test):
            test.app.transition('start', 'ui')

        def app_gone(test):
            test.alive = False

        cases = [
            dict(name='the user started a program', input=(ui_click, None), expectation='console_held_by_user'),
            dict(name='no lease', input=(None, 'not-a-lease'), expectation='lease_invalid'),
            dict(name='the app is not running', input=(app_gone, None), expectation='app_unavailable'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                token = self.acquired()
                setup, other_token = case['input']
                if setup is not None:
                    setup(self)
                record = self.stop_program(stop_request(other_token or token))
                self.assertEqual((record['status'], record['refusal']['code']), ('refused', case['expectation']))
                self.assertEqual(self.sent('stop'), [])


class RaceTests(ActionTest):
    def test_ui_takeover_during_start(self):
        token = self.acquired()
        self.app.after['start'] = lambda app: app.transition('stop', 'ui')
        record = self.run_program(self.run_req(token))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'console_held_by_user'))
        self.assertTrue(record['refusal']['evidence']['journal_ok'])
        self.assertEqual(self.lease(), 'user')
        retry = self.run_program(self.run_req(token))
        self.assertEqual(retry['refusal']['code'], 'console_held_by_user')
        self.assertEqual(len(self.sent()), 1)

    def test_ui_takeover_during_stop(self):
        token = self.acquired()
        self.started(token)
        self.app.stop_ticks = 5
        self.app.after['stop'] = lambda app: app.transition('stop', 'ui', state='stopping')
        record = self.stop_program(stop_request(token))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'console_held_by_user'))
        self.assertEqual(self.lease(), 'user')

    def test_lease_lost_to_the_user_while_running(self):
        token = self.acquired()
        self.started(token)
        self.clock.sleep(1)
        self.app.transition('stop', 'ui')
        record = self.stop_program(stop_request(token))
        self.assertEqual((record['status'], record['refusal']['code']), ('refused', 'console_held_by_user'))
        self.assertEqual(self.sent('stop'), [])

    def test_lease_lost_by_expiry_before_the_start(self):
        token = self.acquired()
        self.clock.sleep(1199)
        self.clock.sleep(1)
        record = self.run_program(self.run_req(token))
        self.assertEqual(record['refusal']['code'], 'lease_expired')
        self.assertEqual(self.sent(), [])

    def test_app_restart_between_requests(self):
        token = self.acquired()
        request = self.run_req(token)
        self.run_program(request)
        self.assertEqual(self.lease(), 'agent_running')
        restarted = stopped_app()
        restarted['instance']['id'] = NEW_INSTANCE
        restarted['written_at'] = self.lab.state['written_at']
        self.lab.write_state(restarted)
        # The retry is answered from the record: no second start on the new app.
        self.assertEqual(self.run_program(request)['status'], 'completed')
        self.assertEqual(len(self.sent()), 1)
        self.assertEqual(self.lease(), 'agent_idle')
        self.assertIn(('lease_changed', 'app_restarted'), self.event_types())
        # The stop finds nothing running on the new app.
        stop = self.stop_program(stop_request(token))
        self.assertEqual((stop['result']['sent'], stop['result']['instance_id']), (False, NEW_INSTANCE))
        # A new request starts on the new app.
        record = self.started(token)
        self.assertEqual(record['result']['instance_id'], NEW_INSTANCE)
        self.assertEqual(self.lease(), 'agent_running')

    def test_app_restart_during_the_start(self):
        token = self.acquired()

        def restart(app):
            app.lab.update_state(**{'instance.id': NEW_INSTANCE, 'program.state': 'stopped',
                                    'program.last_transition': None})
        request = self.run_req(token)
        self.app.hold.add('start')
        self.on_first_sleep(lambda: restart(self.app))
        record = self.run_program(request)
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertEqual(record['refusal']['evidence']['reason'], 'instance_replaced')
        self.assertEqual(self.lease(), 'agent_idle')

    def test_the_run_ends_before_the_start_is_answered(self):
        for source in ('program', 'error'):
            with self.subTest(source):
                self.setUp()
                token = self.acquired()
                self.app.after['start'] = lambda app: app.transition('stop', source)
                record = self.run_program(self.run_req(token))
                self.assertEqual(record['status'], 'completed')
                self.assertEqual(record['result']['ended']['source'], source)
                self.assertEqual(record['result']['correlated_by'], 'time')
                self.assertEqual(self.lease(), 'agent_idle')
                types = self.event_types()
                self.assertIn(('stopped', None), types)
                self.assertNotIn(('lease_changed', 'run_started'), types)

    def test_the_run_ends_before_the_stop_line(self):
        token = self.acquired()
        self.started(token)
        self.on_first_sleep(lambda: self.app.transition('stop', 'program'))
        record = self.stop_program(stop_request(token))
        self.assertEqual(record['status'], 'completed')
        self.assertEqual((record['result']['stopped_by'], record['result']['correlated_by']), ('program', 'time'))
        self.assertEqual(self.lease(), 'agent_idle')

    def test_the_run_ends_after_the_status_read_but_before_the_lease_runs(self):
        """observe applies the run's end before finish_action: the lease must not enter agent_running."""
        token = self.acquired()
        request = self.run_req(token)
        self.app.hold.add('start')
        record = self.run_program(request)
        self.assertEqual(record['status'], 'pending')
        self.app.transition('start', 'lab', request['request_id'])
        self.clock.sleep(1)
        start = self.lab.state['program']['last_transition']
        self.app.transition('stop', 'program')
        self.reader.console_state()  # observe sees the stop first
        app = self.lab.state
        self.plane.finish_action(CLIENT, 'run_program', request['request_id'],
                                 result={'console': 'B'}, run_started=(app['instance']['id'], start))
        self.assertEqual(self.lease(), 'agent_idle')


class TicketTests(ActionTest):
    def test_a_request_the_app_never_took_is_withdrawn(self):
        token = self.acquired()
        request = self.run_req(token)
        self.app.serve = False
        record = self.run_program(request)
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'timeout'))
        self.assertEqual(self.inbox(), [])
        self.app.serve = True
        self.clock.sleep(1)
        self.assertEqual(self.sent(), [])
        self.assertEqual(self.lab.state['program']['state'], 'stopped')

    def test_a_taken_request_stays_pending_and_settles_after_a_daemon_restart(self):
        token = self.acquired()
        request = self.run_req(token)
        self.app.hold.add('start')
        record = self.run_program(request)
        self.assertEqual(record['status'], 'pending')
        self.open_daemon()
        self.app.transition('start', 'lab', request['request_id'])
        self.actions.settle_pending('B')
        settled = self.plane.request_status(CLIENT, 'B', request['request_id'])
        self.assertEqual(settled['status'], 'completed')
        self.assertEqual(self.lease(), 'agent_running')
        self.assertEqual(self.run_program(request), self.plane.record(CLIENT, 'run_program', request['request_id']))

    def test_a_pending_request_fails_when_the_app_exits(self):
        token = self.acquired()
        self.app.hold.add('start')
        request = self.run_req(token)
        self.assertEqual(self.run_program(request)['status'], 'pending')
        self.alive = False
        self.actions.settle_pending('B')
        record = self.plane.record(CLIENT, 'run_program', request['request_id'])
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))


class LostRequestTests(ActionTest):
    """A request that never reached the app, or whose outcome nothing names, must not stay pending:
    a pending action refuses every later run and stop on the console."""

    def test_a_failed_inbox_write_is_a_refusal_and_blocks_nothing(self):
        token = self.acquired()
        request = self.run_req(token)
        with mock.patch.object(actions.shot, 'write_request', side_effect=OSError(28, 'No space left on device')):
            record = self.run_program(request)
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'app_unavailable'))
        self.assertEqual(record['refusal']['evidence']['reason'], 'inbox_write_failed')
        self.assertEqual(self.sent(), [])
        self.started(token)

    def test_a_request_never_written_is_settled_as_not_sent(self):
        """The daemon died between storing the record and writing the request."""
        token = self.acquired()
        request = self.run_req(token)
        with mock.patch.object(actions.shot, 'write_request'):
            first = self.run_program(request)
        self.open_daemon()
        self.actions.settle_pending('B')
        record = self.plane.record(CLIENT, 'run_program', request['request_id'])
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'timeout'), first)
        self.assertEqual(record['refusal']['evidence']['reason'], 'never_sent')
        self.started(token)

    def test_a_lab_script_stop_after_the_start_settles_the_run(self):
        token = self.acquired()
        self.app.after['start'] = lambda app: app.transition('stop', 'lab', new_id())
        record = self.run_program(self.run_req(token))
        self.assertEqual(record['status'], 'completed', record)
        self.assertEqual((record['result']['ended']['source'], record['result']['correlated_by']), ('lab', 'time'))
        self.assertEqual(self.lease(), 'agent_idle')

    def test_an_answered_request_that_nothing_matches_fails_after_the_wait(self):
        token = self.acquired()
        # The app answers ok but shows no transition for it (nothing this request can be matched to).
        self.app.after['start'] = lambda app: app.lab.update_state(**{'program.state': 'stopped',
                                                                       'program.last_transition': None})
        record = self.run_program(self.run_req(token))
        self.assertEqual((record['status'], record['refusal']['code']), ('failed', 'timeout'), record)
        self.assertEqual(record['refusal']['evidence']['reason'], 'outcome_unknown')

    def test_a_reused_request_id_is_not_answered_by_an_old_journal(self):
        """A request_id used again after compaction dropped its record gets its own inbox name."""
        token = self.acquired()
        request = self.run_req(token)
        self.app.owner = 'lab-script'
        first = self.run_program(request)
        self.assertEqual((first['status'], first['refusal']['code']), ('failed', 'console_held_by_agent'))
        self.app.owner = None
        with self.plane.db.write() as conn:  # what compaction does to an old finished record
            conn.execute('DELETE FROM requests WHERE request_id = ?', (request['request_id'],))
            conn.execute('DELETE FROM actions WHERE request_id = ?', (request['request_id'],))
        record = self.run_program(request)
        self.assertEqual(record['status'], 'completed', record)
        self.assertEqual(self.lab.state['program']['state'], 'running')


class LeaseWhilePendingTests(ActionTest):
    """The lease cannot end while a run_program is pending: the run would start unleased."""

    def pending_run(self, token):
        request = self.run_req(token)
        self.app.hold.add('start')
        self.assertEqual(self.run_program(request)['status'], 'pending')
        return request

    def let_start_through(self, request):
        self.app.transition('start', 'lab', request['request_id'])
        self.actions.settle_pending('B')
        return self.plane.record(CLIENT, 'run_program', request['request_id'])

    def test_release_is_refused_while_a_run_is_pending(self):
        token = self.acquired()
        request = self.pending_run(token)
        released = self.plane.execute(CLIENT, release(token))
        self.assertEqual((released['status'], released['refusal']['code']), ('refused', 'program_running'))
        self.assertEqual(self.let_start_through(request)['status'], 'completed')
        self.assertEqual(self.lease(), 'agent_running')

    def test_the_lease_does_not_expire_while_a_run_is_pending(self):
        token = self.acquired()
        request = self.pending_run(token)
        self.clock.sleep(1300)
        other = self.plane.execute('claude:other-lane', acquire(owner='agent:other'))
        self.assertEqual((other['status'], other['refusal']['code']), ('refused', 'console_held_by_agent'))
        self.assertEqual(self.let_start_through(request)['status'], 'completed')
        self.assertEqual(self.lease(), 'agent_running')

    def test_the_lease_expires_once_the_pending_run_fails(self):
        token = self.acquired()
        request = self.pending_run(token)
        self.clock.sleep(1300)
        self.alive = False  # the app exits: the run fails
        self.actions.settle_pending('B')
        self.assertEqual(self.plane.record(CLIENT, 'run_program', request['request_id'])['status'], 'failed')
        self.reader.console_state()
        self.assertEqual(self.lease(), 'free')


class McpTests(ActionTest):
    def test_action_mode_keeps_the_read_instructions(self):
        server = mcp_server.LabMcp({'B': self.reader}, control_plane=self.plane, actions=self.actions)
        text = server.initialize_result({})['instructions']
        self.assertIn('Unknown observations are never success', text)
        self.assertIn('run_program', text)

    def test_the_tools_are_listed_only_with_actions(self):
        cases = [
            dict(name='actions', input=dict(control_plane=True, actions=True), expectation=True),
            dict(name='control plane only', input=dict(control_plane=True, actions=False), expectation=False),
        ]
        for case in cases:
            with self.subTest(case['name']):
                server = mcp_server.LabMcp({'B': self.reader}, control_plane=self.plane,
                                           actions=self.actions if case['input']['actions'] else None)
                names = [t['name'] for t in server.call('tools/list', {})['tools']]
                self.assertEqual({'preflight_program', 'run_program', 'stop_program', 'cfr_batch'} <= set(names),
                                 case['expectation'])
                self.assertEqual(bool({'preflight_program', 'run_program', 'stop_program', 'cfr_batch'} & set(names)),
                                 case['expectation'])

    def test_read_only_mode_refuses_actions(self):
        with self.assertRaises(ValueError):
            mcp_server.LabMcp({'B': self.reader}, read_only=True, actions=self.actions)

    def test_run_and_stop_through_tools_call(self):
        server = mcp_server.LabMcp({'B': self.reader}, control_plane=self.plane, actions=self.actions)
        token = self.acquired()
        checked = server.call('tools/call', {'name': 'preflight_program', 'arguments': {
            'console': 'B', 'request_id': new_id(), 'lease_token': token, 'panel': PANEL, 'options': {}}},
            client=CLIENT)
        self.assertFalse(checked['isError'], checked)
        preflight_token = checked['structuredContent']['result']['preflight_token']
        run = server.call('tools/call', {'name': 'run_program', 'arguments': {
            'console': 'B', 'request_id': new_id(), 'lease_token': token, 'panel': PANEL, 'options': {},
            'preflight_token': preflight_token}}, client=CLIENT)
        self.assertFalse(run['isError'], run)
        self.assertEqual(run['structuredContent']['status'], 'completed')
        refused = server.call('tools/call', {'name': 'run_program', 'arguments': {
            'console': 'B', 'request_id': new_id(), 'lease_token': token, 'panel': PANEL, 'options': {},
            'preflight_token': preflight_token}}, client=CLIENT)
        self.assertTrue(refused['isError'])
        self.assertEqual(refused['structuredContent']['refusal']['code'], 'program_running')
        contracts.validate('refusal', refused['structuredContent'])
        stop = server.call('tools/call', {'name': 'stop_program', 'arguments': {
            'console': 'B', 'request_id': new_id(), 'lease_token': token}}, client=CLIENT)
        self.assertEqual(stop['structuredContent']['status'], 'completed')
        self.assertNotIn(token, str(server.call('tools/call', {'name': 'next_events', 'arguments': {
            'console': 'B', 'after_seq': 0, 'limit': 500}}, client=CLIENT)))


if __name__ == '__main__':
    unittest.main()
