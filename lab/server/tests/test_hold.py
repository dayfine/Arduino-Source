"""The user's hold, `Lab/hold`: reading it, the lease it gives, and only an explicit release ends it.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import contextlib
import io
import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest import mock

from pa_lab import __main__ as cli
from pa_lab import config, control, hold, lab_reader, status, timestamps
from tests.control_fixtures import acquire, app_state, clock, open_plane, renew, temp_db_path
from tests.fake_lab import FakeClock, FakeLab, golden

INSTANCE = golden('app_state', 'running_lab')['instance']['id']


def temp_lab(test) -> Path:
    tmp = tempfile.TemporaryDirectory(prefix='pa-lab-hold-')
    test.addCleanup(tmp.cleanup)
    lab = Path(tmp.name) / 'Lab'
    lab.mkdir()
    return lab


class ReadHoldTests(unittest.TestCase):
    def test_read_hold(self):
        cases = [
            dict(name='written by hold.sh',
                 input='owner=user\nreason=hold_sh\nacquired_at=2026-11-15T21:04:05.000Z\n',
                 expectation={'owner': 'user', 'reason': 'hold_sh', 'acquired_at': '2026-11-15T21:04:05.000Z'}),
            dict(name='written by the app, CRLF',
                 input='owner=user\r\nreason=user_input\r\nacquired_at=2026-11-15T21:04:05.120Z\r\n',
                 expectation={'owner': 'user', 'reason': 'user_input', 'acquired_at': '2026-11-15T21:04:05.120Z'}),
            dict(name='time not a contract timestamp: the file time',
                 input='owner=user\nreason=hold_sh\nacquired_at=yesterday\n',
                 expectation={'owner': 'user', 'reason': 'hold_sh', 'acquired_at': '1970-01-01T00:16:40.000Z'}),
            dict(name='no owner', input='reason=hold_sh\n', expectation=None),
            dict(name='empty file', input='', expectation=None),
            dict(name='no file', input=None, expectation=None),
        ]
        for case in cases:
            with self.subTest(case['name']):
                lab = temp_lab(self)
                if case['input'] is not None:
                    (lab / 'hold').write_bytes(case['input'].encode('utf-8'))
                    os.utime(lab / 'hold', (1000, 1000))
                self.assertEqual(hold.read_hold(lab), case['expectation'])

    def test_an_unreadable_hold_is_a_hold(self):
        cases = [
            dict(name='not UTF-8', input=b'owner=us\xffer\n'),
            dict(name='a folder', input=None),
        ]
        for case in cases:
            with self.subTest(case['name']):
                lab = temp_lab(self)
                if case['input'] is None:
                    (lab / 'hold').mkdir()
                else:
                    (lab / 'hold').write_bytes(case['input'])
                os.utime(lab / 'hold', (1000, 1000))
                self.assertEqual(hold.read_hold(lab), {'owner': 'unknown', 'reason': 'unreadable',
                                                       'acquired_at': '1970-01-01T00:16:40.000Z'})

    def test_write_keeps_the_hold_in_force_and_clear_removes_it(self):
        lab = temp_lab(self)
        first = hold.write_hold(lab, 'hold_sh', now=lambda: 1000.0)
        self.assertEqual(hold.write_hold(lab, 'operator_claim', now=lambda: 2000.0), first)
        self.assertEqual(hold.read_hold(lab), first)
        self.assertTrue(hold.clear_hold(lab))
        self.assertIsNone(hold.read_hold(lab))
        self.assertFalse(hold.clear_hold(lab))


class ReportedLeaseTests(unittest.TestCase):
    """The read-only server (no store): the lease comes from the app's state and `Lab/hold`."""

    def reader(self, lab, at='2026-11-14T06:10:12.400Z'):
        return lab_reader.ConsoleReader('B', lab.dir, profile='lab-B', now=FakeClock(at).now,
                                        pid_alive=lambda pid: True)

    def test_a_hold_makes_the_console_the_users_while_a_lab_run_is_shown(self):
        lab = FakeLab(self)
        app = golden('app_state', 'running_lab')
        lab.write_state(app)
        (lab.dir / 'hold').write_text('owner=user\nreason=user_input\nacquired_at=2026-11-14T06:10:11.000Z\n')
        state = self.reader(lab, app['written_at']).console_state()
        self.assertEqual(state['lease'], {'state': 'user', 'owner': 'user', 'acquired_at': '2026-11-14T06:10:11.000Z',
                                          'expires_at': None})
        self.assertEqual(state['blocked'][0]['code'], 'console_held_by_user')

    def test_a_hold_shows_while_the_app_is_down(self):
        lab = FakeLab(self)
        (lab.dir / 'hold').write_text('owner=user\nreason=hold_sh\nacquired_at=2026-11-14T06:10:11.000Z\n')
        state = self.reader(lab).console_state()
        self.assertEqual(state['availability']['state'], 'unavailable')
        self.assertEqual(state['lease']['state'], 'user')


class StoredLeaseTests(unittest.TestCase):
    """With the store: a hold revokes an agent lease, and only release_user ends the user lease."""

    HOLD = {'owner': 'user', 'reason': 'user_input', 'acquired_at': '2026-11-14T06:00:05.000Z'}

    def setUp(self):
        self.clock = clock()
        self.plane = open_plane(self, temp_db_path(self), self.clock)

    def test_a_hold_revokes_the_agent_lease(self):
        token = self.plane.execute('claude:frlg-lane', acquire())['result']['lease_token']
        lease = self.plane.observe('B', app_state(None, 'stopped', INSTANCE), self.HOLD)
        self.assertEqual(lease, {'state': 'user', 'owner': 'user', 'acquired_at': self.HOLD['acquired_at'],
                                 'expires_at': None})
        changed = [e['payload']['reason'] for e in self.plane.next_events('reader', 'B', 0)['events']
                   if e['type'] == 'lease_changed']
        self.assertEqual(changed[-1], 'ui_takeover')
        self.assertEqual(self.plane.execute('claude:frlg-lane', renew(token))['refusal']['code'],
                         'console_held_by_user')

    def test_the_hold_does_not_end_by_itself(self):
        self.plane.observe('B', app_state(None, 'stopped', INSTANCE), self.HOLD)
        self.clock.sleep(30 * 86400)
        self.assertEqual(self.plane.observe('B', app_state(None, 'stopped', INSTANCE), None)['state'], 'user')
        self.assertEqual(self.plane.release_user('B')['state'], 'free')
        self.assertEqual(self.plane.observe('B', app_state(None, 'stopped', INSTANCE), None)['state'], 'free')

    def test_a_hold_read_before_the_release_does_not_come_back(self):
        self.plane.observe('B', app_state(None, 'stopped', INSTANCE), self.HOLD)
        self.clock.sleep(60)
        self.assertEqual(self.plane.release_user('B')['state'], 'free')
        # An observe() that read the file just before release-user removed it commits after the release.
        self.assertEqual(self.plane.observe('B', app_state(None, 'stopped', INSTANCE), self.HOLD)['state'], 'free')

    def test_a_hold_put_on_after_the_release_holds(self):
        self.plane.observe('B', app_state(None, 'stopped', INSTANCE), self.HOLD)
        self.plane.release_user('B')
        self.clock.sleep(60)
        later = dict(self.HOLD, acquired_at=timestamps.format(self.clock.now()))
        self.assertEqual(self.plane.observe('B', app_state(None, 'stopped', INSTANCE), later)['state'], 'user')

    def test_a_refused_release_still_outlasts_a_hold_read_before_it(self):
        self.clock.sleep(60)
        with self.assertRaises(control.Refused):
            self.plane.release_user('B')
        self.assertEqual(self.plane.observe('B', app_state(None, 'stopped', INSTANCE), self.HOLD)['state'], 'free')


class LeaseCommandTests(unittest.TestCase):
    """`python3 -m pa_lab lease claim-user|release-user` writes and removes `Lab/hold` too."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix='pa-lab-hold-cli-')
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        self.lab = root / 'Lab'
        self.lab.mkdir()
        token = root / 'token'
        config.create_token(token)
        self.config = root / 'server.json'
        self.config.write_text(json.dumps({
            'schema': 1, 'host': '127.0.0.1', 'port': 8765, 'token_file': str(token),
            'db_file': str(root / 'control.sqlite'),
            'consoles': {'B': {'lab_dir': str(self.lab), 'profile': 'lab-B'}}}), encoding='utf-8')

    def lease(self, action):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main(['lease', '--config', str(self.config), action])
        return code, out.getvalue()

    def test_claim_then_release(self):
        self.assertEqual(self.lease('claim-user')[0], 0)
        self.assertEqual(hold.read_hold(self.lab)['reason'], 'operator_claim')
        self.assertEqual(self.lease('release-user')[0], 0)
        self.assertIsNone(hold.read_hold(self.lab))
        self.assertIn('"state": "free"', self.lease('show')[1])

    def test_a_refused_claim_writes_no_hold(self):
        refused = control.Refused('invalid_arguments', 'no')
        with mock.patch.object(control.ControlPlane, 'claim_user', side_effect=refused):
            code, out = self.lease('claim-user')
        self.assertEqual(code, 1, out)
        self.assertIsNone(hold.read_hold(self.lab))

    def test_release_of_a_hold_the_store_never_saw(self):
        (self.lab / 'hold').write_text('owner=user\nreason=user_input\n')
        code, out = self.lease('release-user')
        self.assertEqual(code, 0, out)
        self.assertIsNone(hold.read_hold(self.lab))


class ConsoleStatusTests(unittest.TestCase):
    def test_console_status_names_the_holder(self):
        tmp = tempfile.TemporaryDirectory(prefix='pa-lab-hold-status-')
        self.addCleanup(tmp.cleanup)
        profile = Path(tmp.name) / 'lab-B'
        (profile / 'Lab').mkdir(parents=True)
        (profile / 'Lab' / 'hold').write_text('owner=user\nreason=ui_start\nacquired_at=2026-11-15T21:04:05.120Z\n')
        doc = status.ProfileStatus('B', profile, find_app=lambda name: None).console_status()
        self.assertEqual(doc['hold'], {'owner': 'user', 'reason': 'ui_start', 'acquired_at': '2026-11-15T21:04:05.120Z'})
        self.assertEqual(doc['console_state']['lease']['owner'], 'user')

    def test_console_status_survives_an_unreadable_hold(self):
        tmp = tempfile.TemporaryDirectory(prefix='pa-lab-hold-status-')
        self.addCleanup(tmp.cleanup)
        profile = Path(tmp.name) / 'lab-B'
        (profile / 'Lab').mkdir(parents=True)
        (profile / 'Lab' / 'hold').write_bytes(b'owner=\xff\n')
        doc = status.ProfileStatus('B', profile, find_app=lambda name: None).console_status()
        self.assertEqual(doc['hold']['reason'], 'unreadable')
        self.assertEqual(doc['console_state']['lease']['state'], 'user')


if __name__ == '__main__':
    unittest.main()
