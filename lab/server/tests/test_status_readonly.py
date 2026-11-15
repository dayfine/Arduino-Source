"""console_status and the read-only stdio server against a fake profile folder.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import io
import json
import os
from pathlib import Path
import tempfile
import unittest

from pa_lab import control, db, mcp_server, status, stdio_server

LOG = """2026-11-15 00:10:00.000001 - [Console 0]: Program State: RUNNING
2026-11-15 00:10:01.000001 - [Console 0]: Starting Program: PokemonFRLG:AutoStory
2026-11-15 00:10:05.000001 - [Console 0]: issue_buttons(): B, delay = 580ms, hold = 80ms, cooldown = 0ms
2026-11-15 00:10:06.000001 - [Console 0]: issue_nop(): delay = 80ms
2026-11-15 00:11:00.000001 - [Console 0]: [Checkpoint] 10.01: failed after 54 s
2026-11-15 00:11:01.000001 - [Console 0]: Program State: STOPPED
"""


def fake_profile(root: Path) -> Path:
    profile = root / 'lab-B'
    (profile / 'Lab' / 'inbox').mkdir(parents=True)
    (profile / 'Lab' / 'state').write_text(
        'seq=keepawake35\nstate=STOPPED\nprogram=PokemonFRLG:AutoStory\ncontroller=ready\nowner=secret\nerror=\n')
    (profile / 'SerialPrograms.log').write_text(LOG)
    failures = profile / 'Screenshots' / 'AutoStory' / 'failures'
    failures.mkdir(parents=True)
    (failures / 'old.png').write_bytes(b'x')
    os.utime(failures / 'old.png', (1000, 1000))
    (failures / 'new.png').write_bytes(b'x')
    os.utime(failures / 'new.png', (2000, 2000))
    (profile / 'Screenshots' / 'shot.jpg').write_bytes(b'x')
    os.utime(profile / 'Screenshots' / 'shot.jpg', (3000, 3000))
    return profile


class ConsoleStatusTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.profile = fake_profile(self.root)
        keepawake = self.root / 'keepawake.log'
        keepawake.write_text('00:09:14 idle 182s: pressed B\n00:12:15 idle 181s: pressed B\n')
        self.status = status.ProfileStatus('B', self.profile, keepawake_log=keepawake,
                                           find_app=lambda profile: {'pid': 42, 'deployment': 'slot-B/x'})

    def tearDown(self):
        self.tmp.cleanup()

    def test_reports_what_checkin_reports(self):
        doc = self.status.console_status(3)
        self.assertEqual(doc['app'], {'pid': 42, 'deployment': 'slot-B/x'})
        self.assertEqual(doc['lab_state']['state'], 'STOPPED')
        self.assertNotIn('owner', doc['lab_state'])
        self.assertIn('Program State: STOPPED', doc['program_state'])
        self.assertEqual(len(doc['last_run']), 2)
        self.assertIn('Starting Program', doc['last_run'][0])
        self.assertIn('10.01: failed', doc['last_run'][1])
        self.assertEqual(doc['last_press'], '00:10:05')
        self.assertEqual(doc['keepawake'], '00:12:15 idle 181s: pressed B')
        self.assertTrue(doc['newest_failure_frame'].endswith('failures/new.png'))
        self.assertTrue(doc['newest_screenshot'].endswith('Screenshots/shot.jpg'))
        self.assertEqual(len(doc['log_tail']), 3)
        self.assertEqual(doc['console_state']['availability'],
                         {'state': 'unavailable', 'reason': 'no_state_file'})

    def test_empty_profile_reports_nothing_known(self):
        empty = self.root / 'empty'
        empty.mkdir()
        doc = status.ProfileStatus('B', empty, find_app=lambda profile: None).console_status()
        self.assertIsNone(doc['lab_state'])
        self.assertIsNone(doc['program_state'])
        self.assertEqual(doc['last_run'], [])
        self.assertIsNone(doc['newest_failure_frame'])
        self.assertIsNone(doc['newest_screenshot'])


class ReadOnlyServerTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.profile = fake_profile(Path(self.tmp.name))
        self.mcp = stdio_server.make_mcp(self.profile)

    def tearDown(self):
        self.tmp.cleanup()

    def test_lists_only_console_status(self):
        self.assertEqual([t['name'] for t in self.mcp.call('tools/list', {})['tools']], ['console_status'])

    def test_inbox_and_control_tools_are_refused_and_write_nothing(self):
        for name in ('shot', 'describe_program', 'game_open', 'ensure_game', 'acquire_console', 'next_events'):
            with self.assertRaises(mcp_server.RpcError, msg=name):
                self.mcp.call('tools/call', {'name': name, 'arguments': {'console': 'B'}})
        self.assertEqual(list((self.profile / 'Lab' / 'inbox').iterdir()), [])

    def test_read_only_refuses_a_control_plane(self):
        with tempfile.TemporaryDirectory() as d:
            plane = control.ControlPlane(db.Database(Path(d) / 'c.sqlite'), ['B'])
            with self.assertRaises(ValueError):
                mcp_server.LabMcp({}, control_plane=plane, read_only=True)

    def test_console_status_refuses_bad_arguments(self):
        for arguments in ({'console': 'A'}, {'console': 'B', 'log_lines': 1000}, {'console': 'B', 'x': 1}):
            result = self.mcp.call('tools/call', {'name': 'console_status', 'arguments': arguments})
            self.assertTrue(result['isError'], arguments)

    def test_stdio_round_trip(self):
        requests = [
            {'jsonrpc': '2.0', 'id': 1, 'method': 'initialize',
             'params': {'protocolVersion': '2025-06-18', 'capabilities': {}, 'clientInfo': {'name': 't'}}},
            {'jsonrpc': '2.0', 'method': 'notifications/initialized'},
            {'jsonrpc': '2.0', 'id': 2, 'method': 'tools/call',
             'params': {'name': 'console_status', 'arguments': {'console': 'B', 'log_lines': 1}}},
            {'jsonrpc': '2.0', 'id': 3, 'method': 'resources/read', 'params': {'uri': 'console://B/state'}},
        ]
        out = io.StringIO()
        stdio_server.serve(self.mcp, io.StringIO(''.join(json.dumps(r) + '\n' for r in requests) + 'not json\n'), out)
        replies = [json.loads(line) for line in out.getvalue().splitlines()]
        self.assertEqual([r['id'] for r in replies], [1, 2, 3, None])
        self.assertEqual(replies[0]['result']['serverInfo']['name'], 'pa-lab-readonly')
        self.assertFalse(replies[1]['result']['isError'])
        self.assertEqual(replies[1]['result']['structuredContent']['lab_state']['state'], 'STOPPED')
        self.assertIn('no_state_file', replies[2]['result']['contents'][0]['text'])
        self.assertEqual(replies[3]['error']['code'], mcp_server.PARSE_ERROR)


if __name__ == '__main__':
    unittest.main()
