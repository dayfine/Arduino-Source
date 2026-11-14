"""The control tools over the MCP endpoint (Slice 3, fork #131): lease calls with request ids,
console://B/state reading the stored lease, a UI click in the fake app becoming the user lease,
and event catch-up."""

import json
import threading
import time
import unittest
import uuid

from pa_lab import control, db, lab_reader, mcp_server, timestamps
from tests.control_fixtures import temp_db_path
from tests.fake_lab import FakeApp, FakeLab, golden
from tests.test_mcp_server import TOKEN, Client, WallClock


class ControlServerTest(unittest.TestCase):
    def setUp(self):
        self.lab = FakeLab(self)
        state = golden('app_state', 'stopped_ui')
        state['program']['last_transition'] = None
        state['written_at'] = timestamps.format(time.time())
        state['video']['captured_at'] = state['written_at']
        self.lab.write_state(state)
        self.app = FakeApp(self.lab, WallClock())
        database = db.Database(temp_db_path(self))
        self.addCleanup(database.close)
        self.plane = control.ControlPlane(database, ['B'])
        console = lab_reader.ConsoleReader('B', self.lab.dir, profile='lab-B', pid_alive=lambda pid: True,
                                           control=self.plane)
        self.server = mcp_server.make_server(0, TOKEN, {'B': console}, control_plane=self.plane)
        threading.Thread(target=self.server.serve_forever, daemon=True).start()
        self.addCleanup(self._shutdown)
        self.client = Client(self.server.server_address[1])
        self.client.initialize()

    def _shutdown(self):
        self.server.shutdown()
        self.server.server_close()

    def heartbeat(self, **changes):
        changes['written_at'] = timestamps.format(time.time())
        self.lab.update_state(**changes)

    def test_the_control_tools_are_listed(self):
        _, body = self.client.rpc('tools/list')
        names = [t['name'] for t in body['result']['tools']]
        self.assertEqual(names, ['shot', 'ensure_game', 'acquire_console', 'renew_console', 'release_console',
                                 'next_events', 'request_status'])

    def test_acquire_then_state_shows_the_lease_and_a_retry_matches(self):
        request_id = str(uuid.uuid4())
        first = self.client.tool('acquire_console', console='B', request_id=request_id, owner='agent:frlg-lane')
        again = self.client.tool('acquire_console', console='B', request_id=request_id, owner='agent:frlg-lane')
        self.assertFalse(first['isError'])
        self.assertEqual(first['structuredContent'], again['structuredContent'])
        self.heartbeat()
        lease = self.client.read('console://B/state')['lease']
        self.assertEqual((lease['state'], lease['owner']), ('agent_idle', 'agent:frlg-lane'))
        status = self.client.tool('request_status', console='B', request_id=request_id)['structuredContent']
        self.assertNotIn('lease_token', status['result'])

    def test_a_ui_click_in_the_app_becomes_the_user_lease(self):
        token = self.client.tool('acquire_console', console='B', request_id=str(uuid.uuid4()),
                                 owner='agent:frlg-lane')['structuredContent']['result']['lease_token']
        at = timestamps.format(time.time())
        self.heartbeat(**{'program.last_transition': {'action': 'stop', 'source': 'ui', 'request_id': None,
                                                      'at': at}})
        state = self.client.read('console://B/state')
        self.assertEqual(state['lease']['state'], 'user')
        self.assertEqual(state['blocked'][0]['code'], 'console_held_by_user')
        renewed = self.client.tool('renew_console', console='B', request_id=str(uuid.uuid4()), lease_token=token)
        self.assertTrue(renewed['isError'])
        self.assertEqual(renewed['structuredContent']['refusal']['code'], 'console_held_by_user')
        page = self.client.tool('next_events', console='B', after_seq=0)['structuredContent']
        self.assertEqual([e['type'] for e in page['events']],
                         ['lease_changed', 'request_completed', 'stopped', 'lease_changed', 'request_failed'])
        self.assertNotIn(token, json.dumps(page))

    def test_bad_control_calls_are_refusals(self):
        cases = [
            dict(name='console A', input=('acquire_console', dict(console='A', request_id=str(uuid.uuid4()),
                                                                  owner='x')), expectation='console_not_allowed'),
            dict(name='request id not a uuid', input=('acquire_console', dict(console='B', request_id='r1', owner='x')),
                 expectation='invalid_arguments'),
            dict(name='unknown next_events argument', input=('next_events', dict(console='B', after_seq=0, wait=1)),
                 expectation='invalid_arguments'),
            dict(name='wait over 60 s', input=('next_events', dict(console='B', after_seq=0, timeout_s=61)),
                 expectation='invalid_arguments'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                name, arguments = case['input']
                result = self.client.tool(name, **arguments)
                self.assertTrue(result['isError'])
                self.assertEqual(result['structuredContent']['refusal']['code'], case['expectation'])


if __name__ == '__main__':
    unittest.main()
