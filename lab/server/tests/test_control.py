"""The control plane on one SQLite file: leases, retries, user takeover, restart reconciliation,
events and compaction (Slice 3 of fork #131). A FakeClock drives every TTL; restarts reopen the
same file with a new ControlPlane, as a restarted daemon would.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import threading
import time
import unittest

from pa_lab import contracts, control
from tests.control_fixtures import (acquire, app_state, clock, new_id, open_plane, release, renew,
                                    temp_db_path, transition)

INSTANCE = 'd8ea6a9d-1eab-4a40-b355-d88072b289a1'
NEW_INSTANCE = '0b5e3f6c-9d2a-4e71-8c40-6f1a2b3c4d5e'


class PlaneTest(unittest.TestCase):
    def setUp(self):
        self.clock = clock()
        self.path = temp_db_path(self)
        self.plane = open_plane(self, self.path, self.clock)

    def restart(self, retention=None):
        """A new daemon on the same file: nothing carried over but the SQLite rows."""
        self.plane = open_plane(self, self.path, self.clock, retention)
        return self.plane

    def acquired(self, **arguments):
        record = self.plane.execute('claude:frlg-lane', acquire(**arguments))
        self.assertEqual(record['status'], 'completed', record)
        return record['result']['lease_token']

    def events(self, after=0):
        return self.plane.next_events('reader', 'B', after)['events']

    def types(self, after=0):
        return [(e['type'], e['payload'].get('reason')) for e in self.events(after)]


class LeaseTests(PlaneTest):
    def test_acquire_gives_an_idle_lease_with_the_default_ttl(self):
        record = self.plane.execute('claude:frlg-lane', acquire())
        self.assertEqual(record['result']['state'], 'agent_idle')
        self.assertEqual(record['result']['expires_at'], '2026-11-14T06:20:00.000Z')
        self.assertEqual(record['arguments'], {'owner': 'agent:frlg-lane', 'ttl_s': 1200})
        self.assertEqual(self.plane.lease('B')['owner'], 'agent:frlg-lane')

    def test_a_held_console_is_refused_to_everyone_else(self):
        self.acquired()
        cases = [
            dict(name='another agent', input=('claude:bdsp-lane', 'agent:bdsp-lane'),
                 expectation='console_held_by_agent'),
            dict(name='the same owner name without the token', input=('claude:frlg-lane', 'agent:frlg-lane'),
                 expectation='console_held_by_agent'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                client, owner = case['input']
                record = self.plane.execute(client, acquire(owner))
                self.assertEqual((record['status'], record['refusal']['code']), ('refused', case['expectation']))

    def test_renew_moves_the_expiry(self):
        token = self.acquired()
        self.clock.sleep(1000)
        record = self.plane.execute('claude:frlg-lane', renew(token, ttl_s=600))
        self.assertEqual(record['result']['expires_at'], '2026-11-14T06:26:40.000Z')
        self.clock.sleep(500)
        self.assertEqual(self.plane.lease('B')['state'], 'agent_idle')

    def test_an_idle_lease_expires_after_its_ttl(self):
        token = self.acquired()
        self.clock.sleep(1200)
        self.assertEqual(self.plane.lease('B')['state'], 'free')
        record = self.plane.execute('claude:frlg-lane', renew(token))
        self.assertEqual(record['refusal']['code'], 'lease_expired')
        self.assertIn(('lease_changed', 'expired'), self.types())

    def test_tokens_that_do_not_hold_the_console_are_refused(self):
        token = self.acquired()
        cases = [
            dict(name='made-up token', input=renew('not-a-token'), expectation='lease_invalid'),
            dict(name='release with a made-up token', input=release('not-a-token'), expectation='lease_invalid'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                record = self.plane.execute('claude:frlg-lane', case['input'])
                self.assertEqual(record['refusal']['code'], case['expectation'])
        self.assertEqual(self.plane.execute('claude:frlg-lane', release(token))['result']['state'], 'free')
        self.assertEqual(self.plane.execute('claude:frlg-lane', renew(token))['refusal']['code'], 'lease_invalid')

    def test_a_running_lease_never_expires(self):
        token = self.acquired()
        self.plane.begin_run('B', token, new_id(), INSTANCE)
        self.clock.sleep(10 * 3600)
        self.plane.observe('B', app_state(transition('start', 'lab', '2026-11-14T06:00:01.000Z', new_id()),
                                          'running', INSTANCE))
        lease = self.plane.lease('B')
        self.assertEqual((lease['state'], lease['expires_at']), ('agent_running', None))
        self.assertEqual(self.plane.execute('claude:frlg-lane', release(token))['refusal']['code'],
                         'program_running')

    def test_a_reported_stop_ends_the_run_with_a_fresh_ttl(self):
        token = self.acquired()
        run = new_id()
        self.plane.begin_run('B', token, run, INSTANCE)
        self.clock.sleep(3600)
        self.plane.observe('B', app_state(transition('stop', 'program', '2026-11-14T07:00:00.000Z'), 'stopped',
                                          INSTANCE))
        lease = self.plane.lease('B')
        self.assertEqual((lease['state'], lease['expires_at']), ('agent_idle', '2026-11-14T07:20:00.000Z'))
        self.assertEqual(self.types()[-2:], [('lease_changed', 'run_ended'), ('stopped', None)])


class RetryTests(PlaneTest):
    def test_an_exact_retry_returns_the_stored_result(self):
        request = acquire()
        first = self.plane.execute('claude:frlg-lane', request)
        self.clock.sleep(5)
        again = self.plane.execute('claude:frlg-lane', request)
        self.assertEqual(again, first)
        self.assertEqual([t for t, _ in self.types()].count('request_completed'), 1)

    def test_an_exact_retry_after_a_restart_returns_the_stored_result(self):
        request = acquire()
        first = self.plane.execute('claude:frlg-lane', request)
        self.restart()
        self.assertEqual(self.plane.execute('claude:frlg-lane', request), first)

    def test_a_refusal_is_stable_too(self):
        self.acquired()
        request = acquire('agent:bdsp-lane')
        first = self.plane.execute('claude:bdsp-lane', request)
        self.clock.sleep(1300)  # the console is free now, but the stored answer stands
        self.assertEqual(self.plane.execute('claude:bdsp-lane', request), first)

    def test_reusing_a_request_id_with_other_arguments_is_refused(self):
        request = acquire()
        self.plane.execute('claude:frlg-lane', request)
        cases = [
            dict(name='other owner', input=dict(request, arguments={'owner': 'agent:other'})),
            dict(name='other ttl', input=dict(request, arguments={'owner': 'agent:frlg-lane', 'ttl_s': 600})),
        ]
        for case in cases:
            with self.subTest(case['name']):
                with self.assertRaises(control.Refused) as caught:
                    self.plane.execute('claude:frlg-lane', case['input'])
                self.assertEqual(caught.exception.refusal['code'], 'request_id_reused')

    def test_the_default_ttl_is_part_of_the_normalized_arguments(self):
        request = acquire()
        first = self.plane.execute('claude:frlg-lane', request)
        explicit = dict(request, arguments={'owner': 'agent:frlg-lane', 'ttl_s': 1200})
        self.assertEqual(self.plane.execute('claude:frlg-lane', explicit), first)

    def test_status_reads_and_events_never_show_the_lease_token(self):
        request = acquire()
        token = self.plane.execute('claude:frlg-lane', request)['result']['lease_token']
        status = self.plane.request_status('claude:frlg-lane', request['request_id'])
        self.assertNotIn('lease_token', status['result'])
        for event in self.events():
            self.assertNotIn(token, str(event))

    def test_a_status_read_of_another_clients_request_is_refused(self):
        request = acquire()
        self.plane.execute('claude:frlg-lane', request)
        with self.assertRaises(control.Refused):
            self.plane.request_status('claude:bdsp-lane', request['request_id'])

    def test_an_invalid_request_is_refused_and_not_stored(self):
        cases = [
            dict(name='no owner', input=dict(acquire(), arguments={}), expectation='arguments.owner'),
            dict(name='ttl over an hour', input=acquire(ttl_s=7200), expectation='arguments.ttl_s'),
            dict(name='token on acquire', input=dict(acquire(), lease_token='x'), expectation='lease_token'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                with self.assertRaises(control.Refused) as caught:
                    self.plane.execute('claude:frlg-lane', case['input'])
                self.assertEqual(caught.exception.refusal['field'], case['expectation'])
        self.assertEqual(self.events(), [])


class UserTakeoverTests(PlaneTest):
    UI_STOP = transition('stop', 'ui', '2026-11-14T06:10:10.002Z')

    def test_a_ui_click_revokes_the_agent_lease(self):
        cases = [
            dict(name='idle lease, UI stop', input=('idle', self.UI_STOP)),
            dict(name='running lease, UI stop', input=('running', self.UI_STOP)),
            dict(name='idle lease, UI start', input=('idle', transition('start', 'ui', '2026-11-14T06:10:10.002Z'))),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                held, click = case['input']
                token = self.acquired()
                if held == 'running':
                    self.plane.begin_run('B', token, new_id(), INSTANCE)
                lease = self.plane.observe('B', app_state(click, 'stopped', INSTANCE))
                self.assertEqual(lease, {'state': 'user', 'owner': 'user', 'acquired_at': click['at'],
                                         'expires_at': None})
                self.assertEqual(self.types()[-1], ('lease_changed', 'ui_takeover'))
                self.assertEqual(self.plane.execute('claude:frlg-lane', renew(token))['refusal']['code'],
                                 'console_held_by_user')

    def test_the_user_lease_has_no_timeout_and_survives_restarts(self):
        self.plane.observe('B', app_state(self.UI_STOP, 'stopped', INSTANCE))
        self.clock.sleep(30 * 86400)
        self.restart()
        self.plane.observe('B', app_state(None, 'stopped', NEW_INSTANCE))
        self.assertEqual(self.plane.lease('B')['state'], 'user')
        record = self.plane.execute('claude:frlg-lane', acquire())
        self.assertEqual(record['refusal']['code'], 'console_held_by_user')

    def test_only_the_operator_ends_a_user_lease_and_the_same_click_is_not_applied_again(self):
        app = app_state(self.UI_STOP, 'stopped', INSTANCE)
        self.plane.observe('B', app)
        self.assertEqual(self.plane.release_user('B')['state'], 'free')
        self.assertEqual(self.plane.observe('B', app)['state'], 'free')
        self.acquired()

    def test_the_ui_stop_is_one_stopped_event(self):
        for _ in range(3):
            self.plane.observe('B', app_state(self.UI_STOP, 'stopped', INSTANCE))
        stopped = [e for e in self.events() if e['type'] == 'stopped']
        self.assertEqual([e['payload']['source'] for e in stopped], ['ui'])

    def test_the_operator_can_claim_the_console(self):
        token = self.acquired()
        self.assertEqual(self.plane.claim_user('B')['state'], 'user')
        self.assertEqual(self.plane.execute('claude:frlg-lane', release(token))['refusal']['code'],
                         'console_held_by_user')


class ReconciliationTests(PlaneTest):
    def test_an_idle_lease_and_its_token_survive_a_restart(self):
        token = self.acquired()
        self.restart()
        self.assertEqual(self.plane.execute('claude:frlg-lane', renew(token))['status'], 'completed')

    def test_a_running_lease_survives_a_restart_while_its_app_lives(self):
        token = self.acquired()
        self.plane.begin_run('B', token, new_id(), INSTANCE)
        self.clock.sleep(7200)
        self.restart()
        lease = self.plane.observe('B', app_state(transition('start', 'lab', '2026-11-14T06:00:01.000Z', new_id()),
                                                  'running', INSTANCE))
        self.assertEqual(lease['state'], 'agent_running')

    def test_a_running_lease_whose_app_restarted_goes_back_to_idle(self):
        token = self.acquired()
        self.plane.begin_run('B', token, new_id(), INSTANCE)
        self.restart()
        lease = self.plane.observe('B', app_state(None, 'stopped', NEW_INSTANCE))
        self.assertEqual(lease['state'], 'agent_idle')
        self.assertEqual(self.types()[-1], ('lease_changed', 'app_restarted'))

    def test_a_ui_click_while_the_daemon_was_down_is_applied_on_restart(self):
        token = self.acquired()
        self.restart()
        lease = self.plane.observe('B', app_state(UserTakeoverTests.UI_STOP, 'stopped', INSTANCE))
        self.assertEqual(lease['state'], 'user')
        self.assertEqual(self.plane.execute('claude:frlg-lane', renew(token))['refusal']['code'],
                         'console_held_by_user')

    def test_an_expiry_due_during_downtime_is_applied_once(self):
        self.acquired()
        self.clock.sleep(5000)
        self.restart()
        self.restart()
        self.assertEqual(self.plane.lease('B')['state'], 'free')
        self.assertEqual(self.types().count(('lease_changed', 'expired')), 1)

    def test_tickets_survive_a_restart_and_resolve_once(self):
        ticket = {'schema': 1, 'request_id': new_id(), 'tool': 'stop_program', 'console': 'B',
                  'lease_token': 't', 'arguments': {}}
        self.assertEqual(self.plane.open_ticket('claude:frlg-lane', ticket)['status'], 'pending')
        self.restart()
        self.assertEqual(self.plane.request_status('claude:frlg-lane', ticket['request_id'])['status'], 'pending')
        done = self.plane.resolve_ticket('claude:frlg-lane', 'stop_program', ticket['request_id'],
                                         result={'stopped_at': '2026-11-14T06:00:02.000Z'})
        again = self.plane.resolve_ticket('claude:frlg-lane', 'stop_program', ticket['request_id'],
                                          refusal={'code': 'timeout', 'message': 'late'})
        self.assertEqual((done['status'], again), ('completed', done))
        self.assertEqual([t for t, _ in self.types()], ['request_completed'])


class EventTests(PlaneTest):
    def test_every_event_and_page_meets_the_v1_contract(self):
        token = self.acquired()
        self.plane.begin_run('B', token, new_id(), INSTANCE)
        self.plane.observe('B', app_state(UserTakeoverTests.UI_STOP, 'stopped', INSTANCE))
        page = self.plane.next_events('reader', 'B', 0)
        contracts.validate('event_page', page)
        self.assertEqual([e['seq'] for e in page['events']], list(range(1, len(page['events']) + 1)))

    def test_catch_up_in_small_pages_is_gap_free_across_restarts(self):
        for _ in range(7):
            self.plane.execute('claude:frlg-lane', acquire())
        seen, cursor = [], 0
        for _ in range(20):
            page = self.plane.next_events('reader', 'B', cursor, limit=3)
            if not page['events']:
                break
            seen.extend(e['seq'] for e in page['events'])
            cursor = page['next_after_seq']
            self.restart()
        self.assertEqual(seen, list(range(1, 9)))  # one acquire (2 events), six refusals
        self.assertEqual(self.plane.next_events('reader', 'B', cursor)['next_after_seq'], cursor)

    def test_a_cursor_ahead_of_the_newest_event_is_refused(self):
        with self.assertRaises(control.Refused) as caught:
            self.plane.next_events('reader', 'B', 5)
        self.assertEqual(caught.exception.refusal['field'], 'after_seq')

    def test_a_waiting_reader_wakes_on_a_new_event(self):
        got = {}
        reader = threading.Thread(target=lambda: got.update(page=self.plane.next_events('reader', 'B', 0, 10)))
        reader.start()
        time.sleep(0.1)
        self.plane.execute('claude:frlg-lane', acquire())
        reader.join(5)
        self.assertFalse(reader.is_alive())
        self.assertEqual(got['page']['events'][0]['seq'], 1)

    def test_a_wait_with_nothing_new_returns_the_same_cursor(self):
        self.plane.execute('claude:frlg-lane', acquire())
        started = time.monotonic()
        page = self.plane.next_events('reader', 'B', 2, timeout_s=0.3)
        self.assertGreaterEqual(time.monotonic() - started, 0.25)
        self.assertEqual(page, {'schema': 1, 'events': [], 'next_after_seq': 2})

    def test_seqs_are_never_reused_after_compaction(self):
        self.plane.execute('claude:frlg-lane', acquire())
        self.restart(control.Retention(keep_events=0)).compact()
        self.plane.execute('claude:bdsp-lane', acquire('agent:bdsp-lane'))
        self.assertEqual([e['seq'] for e in self.plane.next_events('reader', 'B', 2)['events']], [3])


class CompactionTests(PlaneTest):
    def fill(self, n):
        for _ in range(n):
            self.plane.execute('claude:frlg-lane', acquire('agent:x'))

    def test_events_after_an_active_cursor_are_kept(self):
        self.fill(10)  # 2 events (acquire) + 9 request_failed = 11
        self.plane.next_events('slow-reader', 'B', 4)
        self.restart(control.Retention(keep_events=0))
        self.assertEqual(self.plane.compact()['events'], 4)
        page = self.plane.next_events('slow-reader', 'B', 4)
        self.assertEqual([e['seq'] for e in page['events']], list(range(5, 12)))

    def test_a_cursor_behind_compaction_is_refused_with_the_resume_point(self):
        self.fill(5)
        self.restart(control.Retention(keep_events=2)).compact()
        with self.assertRaises(control.Refused) as caught:
            self.plane.next_events('late-reader', 'B', 0)
        self.assertEqual(caught.exception.refusal['evidence'], {'compacted_through': 4})
        self.assertEqual(len(self.plane.next_events('late-reader', 'B', 4)['events']), 2)

    def test_an_unused_cursor_stops_protecting_events(self):
        self.fill(5)
        self.plane.next_events('gone-reader', 'B', 0)
        self.clock.sleep(8 * 86400)
        self.restart(control.Retention(keep_events=0))
        self.assertEqual(self.plane.compact(), {'events': 6, 'requests': 5, 'cursors': 1})

    def test_unresolved_tickets_and_their_events_are_kept(self):
        ticket = {'schema': 1, 'request_id': new_id(), 'tool': 'stop_program', 'console': 'B',
                  'lease_token': 't', 'arguments': {}}
        self.plane.open_ticket('claude:frlg-lane', ticket)
        with self.plane.db.write() as conn:  # an event of the in-flight request (Slice 5 emits these)
            self.plane._append(conn, 'stopped', 'B', {'source': 'lab'}, request_id=ticket['request_id'])
        self.fill(3)
        self.clock.sleep(30 * 86400)
        self.restart(control.Retention(keep_events=0)).compact()
        self.assertEqual(self.plane.request_status('claude:frlg-lane', ticket['request_id'])['status'], 'pending')
        kept = self.plane.db.read().execute('SELECT seq, request_id FROM events').fetchall()
        self.assertEqual([(r['seq'], r['request_id']) for r in kept], [(1, ticket['request_id'])])

    def test_compaction_runs_by_itself_after_enough_events(self):
        self.restart(control.Retention(keep_events=3, compact_every=4))
        self.fill(6)
        count = self.plane.db.read().execute('SELECT COUNT(*) FROM events').fetchone()[0]
        self.assertLessEqual(count, 6)


if __name__ == '__main__':
    unittest.main()
