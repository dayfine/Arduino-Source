"""console://B/state and console://B/save from a fake Lab/ folder (Slice 2, fork #131).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import unittest

from pa_lab import contracts, lab_reader
from tests.fake_lab import FakeClock, FakeLab, golden


def reader(lab, clock, alive=True):
    return lab_reader.ConsoleReader('B', lab.dir, profile='lab-B', now=clock.now,
                                    pid_alive=lambda pid: alive)


class ConsoleStateTests(unittest.TestCase):
    def test_stopped_by_ui_is_the_user_lease_golden(self):
        lab = FakeLab(self)
        lab.write_state(golden('app_state', 'stopped_ui'))
        clock = FakeClock('2026-11-14T06:10:12.400Z')
        self.assertEqual(reader(lab, clock).console_state(), golden('console_state', 'available_user_lease'))

    def test_unsupported_schema_is_the_unavailable_golden(self):
        lab = FakeLab(self)
        doc = golden('app_state', 'stopped_ui')
        doc['schema'] = 2
        lab.write_state(doc)
        clock = FakeClock('2026-11-14T08:00:00.000Z')
        self.assertEqual(reader(lab, clock).console_state(), golden('console_state', 'unavailable'))

    def test_unavailable_states(self):
        written_at = '2026-11-14T06:10:12.124Z'
        cases = [
            dict(name='no state file',
                 input=dict(text=None, now=written_at, alive=True),
                 expectation=dict(reason='no_state_file', contains='does not exist')),
            dict(name='partial write',
                 input=dict(text='{"schema": 1, "written_at": "2026-11-14T06:1', now=written_at, alive=True),
                 expectation=dict(reason='malformed', contains='not complete JSON')),
            dict(name='duplicate key',
                 input=dict(text='{"schema": 1, "schema": 1}', now=written_at, alive=True),
                 expectation=dict(reason='malformed', contains='duplicate key')),
            dict(name='NaN',
                 input=dict(text='{"schema": NaN}', now=written_at, alive=True),
                 expectation=dict(reason='malformed', contains='NaN is not JSON')),
            dict(name='contract break',
                 input=dict(edit={'program.state': 'paused'}, now=written_at, alive=True),
                 expectation=dict(reason='malformed', contains="program.state: 'paused' is not one of")),
            dict(name='another profile',
                 input=dict(edit={'instance.profile': 'lab-A'}, now=written_at, alive=True),
                 expectation=dict(reason='malformed', contains="profile 'lab-A'")),
            dict(name='dead pid',
                 input=dict(edit={}, now=written_at, alive=False),
                 expectation=dict(reason='app_not_running', contains='pid 4411')),
            dict(name='stale heartbeat',
                 input=dict(edit={}, now='2026-11-14T06:10:22.200Z', alive=True),
                 expectation=dict(reason='stale_heartbeat', contains='10.1 s ago')),
        ]
        for case in cases:
            with self.subTest(case['name']):
                lab = FakeLab(self)
                given = case['input']
                if 'edit' in given:
                    lab.state = golden('app_state', 'stopped_ui')
                    lab.update_state(**given['edit'])
                elif given['text'] is not None:
                    lab.write_raw(lab.state_path, given['text'])
                state = reader(lab, FakeClock(given['now']), alive=given['alive']).console_state()
                self.assertEqual(state['availability'], {'state': 'unavailable',
                                                         'reason': case['expectation']['reason']})
                self.assertIsNone(state['app'])
                self.assertEqual(state['blocked'][0]['code'], 'app_unavailable')
                self.assertIn(case['expectation']['contains'], state['blocked'][0]['message'])

    def test_partial_write_recovers_on_the_next_whole_file(self):
        lab = FakeLab(self)
        clock = FakeClock('2026-11-14T06:10:12.400Z')
        whole = golden('app_state', 'stopped_ui')
        lab.write_raw(lab.state_path, '{"schema": 1, "writ')
        console = reader(lab, clock)
        self.assertEqual(console.console_state()['availability']['reason'], 'malformed')
        lab.write_state(whole)
        self.assertEqual(console.console_state()['app'], whole)

    def test_app_restart_shows_the_new_instance_with_its_own_sequence(self):
        lab = FakeLab(self)
        clock = FakeClock('2026-11-14T06:10:12.400Z')
        lab.write_state(golden('app_state', 'stopped_ui'))
        console = reader(lab, clock)
        before = console.console_state()['app']
        lab.update_state(**{'instance.id': '11111111-2222-4333-8444-555555555555', 'video.frame_seq': 3,
                            'program.last_transition': None})
        after = console.console_state()
        self.assertEqual(after['availability']['state'], 'available')
        self.assertNotEqual(after['app']['instance']['id'], before['instance']['id'])
        self.assertEqual(after['app']['video']['frame_seq'], 3)
        self.assertEqual(after['lease']['state'], 'free')

    def test_blocked_lists_what_a_run_would_hit(self):
        cases = [
            dict(name='running by lab', input=golden('app_state', 'running_lab'),
                 expectation=['program_running']),
            dict(name='settings panel', input=golden('app_state', 'no_program'),
                 expectation=['panel_not_program', 'video_stale']),
            dict(name='stopped by user', input=golden('app_state', 'stopped_ui'),
                 expectation=['console_held_by_user']),
        ]
        for case in cases:
            with self.subTest(case['name']):
                lab = FakeLab(self)
                lab.write_state(case['input'])
                clock = FakeClock(case['input']['written_at'])
                state = reader(lab, clock).console_state()
                self.assertEqual([b['code'] for b in state['blocked']], case['expectation'])


class SaveResourceTests(unittest.TestCase):
    def setUp(self):
        self.lab = FakeLab(self)
        self.clock = FakeClock('2026-11-14T06:10:12.400Z')

    def test_no_save_file_is_unknown(self):
        self.lab.write_state(golden('app_state', 'stopped_ui'))
        self.assertEqual(reader(self.lab, self.clock).save_resource(), golden('save_resource', 'unknown'))

    def test_hooked_save_is_known(self):
        self.lab.write_state(golden('app_state', 'running_lab'))
        self.clock = FakeClock('2026-11-14T07:02:42.000Z')
        self.lab.write_save(golden('save_fact', 'frlg_checkpoint'))
        self.assertEqual(reader(self.lab, self.clock).save_resource(), golden('save_resource', 'known'))

    def test_ui_stop_after_the_save_makes_it_stale(self):
        self.lab.write_state(golden('app_state', 'stopped_ui'))
        self.lab.write_save(golden('save_fact', 'frlg_checkpoint'))
        self.assertEqual(reader(self.lab, self.clock).save_resource(), golden('save_resource', 'stale_after_user'))

    def test_save_written_after_the_ui_stop_is_known_again(self):
        self.lab.write_state(golden('app_state', 'stopped_ui'))
        fact = golden('save_fact', 'frlg_checkpoint')
        fact['saved_at'] = '2026-11-14T06:11:00.000Z'
        self.lab.write_save(fact)
        self.assertEqual(reader(self.lab, self.clock).save_resource()['status'], 'known')

    def test_unreadable_save_file_is_unavailable_not_unknown(self):
        cases = [
            dict(name='partial write', input='{"schema": 1, "game": "Pok',
                 expectation=dict(reason='malformed', code='app_unavailable')),
            dict(name='future schema', input='{"schema": 3}',
                 expectation=dict(reason='unsupported_schema', code='unsupported_schema')),
        ]
        self.lab.write_state(golden('app_state', 'stopped_ui'))
        for case in cases:
            with self.subTest(case['name']):
                self.lab.write_raw(self.lab.save_path, case['input'])
                with self.assertRaises(lab_reader.Unavailable) as raised:
                    reader(self.lab, self.clock).save_resource()
                self.assertEqual(raised.exception.reason, case['expectation']['reason'])
                self.assertEqual(raised.exception.refusal()['code'], case['expectation']['code'])


if __name__ == '__main__':
    unittest.main()
