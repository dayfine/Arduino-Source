"""The Slice 5 soak check (`python3 -m pa_lab soak-check`): every mutation has exactly one record.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from pa_lab import __main__ as cli
from pa_lab import config, db, soak
from tests.control_fixtures import temp_db_path

T0 = '2026-11-16T10:00:00.000Z'


class SoakCheckTests(unittest.TestCase):
    def setUp(self):
        self.database = db.Database(temp_db_path(self))
        self.addCleanup(self.database.close)

    def request(self, tool, request_id, created_at='2026-11-16T10:00:00.500Z', status='completed', events=1):
        with self.database.write() as conn:
            conn.execute("INSERT INTO requests (client_id, tool, request_id, console, status, created_at, arguments) "
                         "VALUES ('claude', ?, ?, 'B', ?, ?, '{}')", (tool, request_id, status, created_at))
            for _ in range(events):
                conn.execute("INSERT INTO events (type, at, console, request_id, payload) "
                             "VALUES ('request_completed', ?, 'B', ?, ?)",
                             (created_at, request_id, json.dumps({'request_id': request_id, 'tool': tool})))

    def action(self, tool, request_id, inbox_name):
        with self.database.write() as conn:
            conn.execute("INSERT INTO actions (client_id, tool, request_id, console, instance_id, inbox_name, "
                         "written_at) VALUES ('claude', ?, ?, 'B', 'i1', ?, '2026-11-16T10:00:01.000Z')",
                         (tool, request_id, inbox_name))

    def check(self, done_names, since=T0):
        return soak.check(self.database.read(), done_names, since)

    def test_a_clean_run_has_no_mismatch(self):
        cases = [
            dict(name='one request, one event, one action, one done file',
                 input=dict(requests=[('run_program', 'r1', 1)], actions=[('run_program', 'r1', 'palab_run_program_r1_ab')],
                            done=['palab_run_program_r1_ab'])),
            dict(name='read-only requests in done/ have no action row',
                 input=dict(requests=[('preflight_program', 'r1', 1)],
                            actions=[('preflight_program', 'r1', 'palab_preflight_program_r1_ab')],
                            done=['palab_preflight_program_r1_ab', 'palab_describe_program_x1',
                                  'palab_game_open_x2', 'palab_shot_x3'])),
            dict(name='one request id reused across tools',
                 input=dict(requests=[('acquire_console', 'r1', 1), ('preflight_program', 'r1', 1)], actions=[],
                            done=[])),
            dict(name='a request in the same second as T0',
                 input=dict(requests=[('acquire_console', 'r1', 1)], actions=[], done=[])),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                for tool, request_id, events in case['input']['requests']:
                    self.request(tool, request_id, events=events)
                for row in case['input']['actions']:
                    self.action(*row)
                self.assertEqual(self.check(case['input']['done']), soak.CLEAN)

    def test_mismatches(self):
        self.request('run_program', 'r1', events=0)
        self.request('stop_program', 'r2', events=2)
        self.request('run_program', 'r3', status='pending', events=0)
        self.request('run_program', 'old', created_at='2026-11-16T09:59:59.999Z', events=0)
        self.action('stop_program', 'r2', 'palab_stop_program_r2_ab')
        self.assertEqual(self.check(['palab_cfr_batch_r9_cd', 'palab_shot_x3']), {
            'terminal_events': [{'tool': 'run_program', 'request_id': 'r1', 'status': 'completed', 'events': 0},
                                {'tool': 'run_program', 'request_id': 'r3', 'status': 'pending', 'events': 0},
                                {'tool': 'stop_program', 'request_id': 'r2', 'status': 'completed', 'events': 2}],
            'pending': [{'tool': 'run_program', 'request_id': 'r3'}],
            'done_without_action': ['palab_cfr_batch_r9_cd'],
            'action_without_done': ['palab_stop_program_r2_ab'],
        })

    def test_since_must_carry_milliseconds(self):
        with self.assertRaises(ValueError):
            self.check([], since='2026-11-16T10:00:00Z')


class SoakCommandTests(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory(prefix='pa-lab-soak-cli-')
        self.addCleanup(tmp.cleanup)
        root = Path(tmp.name)
        self.done = root / 'Lab' / 'done'
        self.done.mkdir(parents=True)
        token = root / 'token'
        config.create_token(token)
        self.config = root / 'server.json'
        self.config.write_text(json.dumps({
            'schema': 1, 'host': '127.0.0.1', 'port': 8765, 'token_file': str(token),
            'db_file': str(root / 'control.sqlite'),
            'consoles': {'B': {'lab_dir': str(root / 'Lab'), 'profile': 'lab-B'}}}), encoding='utf-8')

    def soak_check(self, since=T0):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            code = cli.main(['soak-check', '--config', str(self.config), '--since', since])
        return code, out.getvalue()

    def test_exit_codes(self):
        (self.done / 'palab_shot_x1.txt').write_text('')
        self.assertEqual(self.soak_check()[0], 0)
        (self.done / 'palab_cfr_batch_r9_cd.txt').write_text('')
        code, out = self.soak_check()
        self.assertEqual(code, 1)
        self.assertEqual(json.loads(out)['done_without_action'], ['palab_cfr_batch_r9_cd'])
        self.assertEqual(self.soak_check(since='2026-11-16T10:00:00Z')[0], 2)


if __name__ == '__main__':
    unittest.main()
