"""shot: a PNG strictly newer than the call, through the app's channel (Slice 2, fork #131).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import unittest

from pa_lab import lab_reader, shot
from tests.fake_lab import PNG, FakeApp, FakeClock, FakeLab, golden

START = '2026-11-14T06:10:12.400Z'
NEW_INSTANCE = '11111111-2222-4333-8444-555555555555'


class ShotTests(unittest.TestCase):
    def setUp(self):
        self.lab = FakeLab(self)
        self.lab.write_state(golden('app_state', 'stopped_ui'))
        self.clock = FakeClock(START)
        self.app = FakeApp(self.lab, self.clock)
        self.console = lab_reader.ConsoleReader('B', self.lab.dir, profile='lab-B', now=self.clock.now,
                                                pid_alive=lambda pid: True)

    def take(self, **arguments):
        return shot.take_shot(self.console, dict(arguments, console='B'), sleep=self.clock.sleep)

    def refusal(self, **arguments):
        with self.assertRaises(shot.Refused) as raised:
            self.take(**arguments)
        return raised.exception.refusal

    def test_returns_a_frame_newer_than_the_call(self):
        result = self.take()
        self.assertEqual(result['png'], PNG)
        self.assertEqual(result['instance_id'], 'd8ea6a9d-1eab-4a40-b355-d88072b289a1')
        self.assertGreater(result['frame_seq_at_least'], 918274)
        self.assertTrue(result['path'].endswith('_frame.png'))
        self.assertEqual(result['call_started_at'], START)

    def test_the_request_only_reads(self):
        self.take()
        self.assertEqual(len(self.app.requests), 1)
        lines = self.app.requests[0].splitlines()
        self.assertRegex(lines[0], r'^request [0-9a-f-]{36}$')
        self.assertEqual(lines[1:], ['shot frame'])

    def test_no_newer_frame_times_out_before_asking_the_app(self):
        self.app.frames = False
        refusal = self.refusal()
        self.assertEqual(refusal, golden('refusal', 'shot_timeout')['refusal'])
        self.assertEqual(self.app.requests, [])

    def test_sequence_going_back_in_one_instance_is_not_newer(self):
        self.app.frames = False
        self.lab.update_state(**{'video.frame_seq': 5})
        with self.assertRaises(shot.Refused) as raised:
            shot.take_shot(self.console, {'console': 'B', 'timeout_ms': 300}, sleep=self._reset_then_sleep)
        self.assertEqual(raised.exception.refusal['code'], 'timeout')

    def _reset_then_sleep(self, seconds):
        self.lab.update_state(**{'video.frame_seq': 2})
        self.clock.sleep(seconds)

    def test_app_restart_while_waiting_is_refused(self):
        self.app.frames = False
        self.clock.on_sleep.append(lambda: self.lab.update_state(**{'instance.id': NEW_INSTANCE,
                                                                    'video.frame_seq': 1}))
        refusal = self.refusal()
        self.assertEqual(refusal['code'], 'app_unavailable')
        self.assertEqual(refusal['evidence']['reason'], 'instance_replaced')
        self.assertEqual(refusal['evidence']['new_instance_id'], NEW_INSTANCE)

    def test_partial_state_file_while_waiting_is_tolerated(self):
        torn = {'n': 0}

        def tear_once():
            if torn['n'] == 0:
                self.lab.write_raw(self.lab.state_path, '{"schema": 1, "writ')
            torn['n'] += 1
        self.clock.on_sleep.append(tear_once)
        self.assertEqual(self.take()['png'], PNG)

    def test_busy_app_times_out_and_the_request_is_withdrawn(self):
        self.app.serve = False
        refusal = self.refusal(timeout_ms=1000)
        self.assertEqual(refusal['code'], 'timeout')
        self.assertIn('did not answer', refusal['message'])
        self.assertEqual(list((self.lab.dir / 'inbox').glob('*.txt')), [])

    def test_app_failures(self):
        cases = [
            dict(name='journal error', input=dict(journal_error='Controller is not ready.'),
                 expectation=dict(code='app_unavailable', contains='Controller is not ready.')),
            dict(name='no frame', input=dict(no_frame=True),
                 expectation=dict(code='video_stale', contains='no video frame')),
            dict(name='truncated png', input=dict(png=PNG[:20]),
                 expectation=dict(code='app_unavailable', contains='not a complete PNG')),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                for knob, value in case['input'].items():
                    setattr(self.app, knob, value)
                refusal = self.refusal()
                self.assertEqual(refusal['code'], case['expectation']['code'])
                self.assertIn(case['expectation']['contains'], refusal['message'])

    def test_refused_before_waiting(self):
        cases = [
            dict(name='video missing', input={'video.state': 'missing', 'video.frame_seq': None,
                                              'video.captured_at': None},
                 expectation='video_stale'),
            dict(name='black screen', input={'video.signal': 'black'}, expectation='video_bad_signal'),
            dict(name='controller not ready', input={'controller.state': 'not_connected'},
                 expectation='controller_not_ready'),
            dict(name='settings panel', input={'panel.kind': 'other', 'panel.id': None,
                                               'program.state': 'no_program'},
                 expectation='panel_not_program'),
            dict(name='future schema', input={'schema': 2}, expectation='unsupported_schema'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.lab.update_state(**case['input'])
                self.assertEqual(self.refusal()['code'], case['expectation'])
                self.assertEqual(self.app.requests, [])

    def test_newer_frame_older_than_max_age_is_stale(self):
        self.app.frames = False
        self.clock.on_sleep.append(lambda: self.lab.update_state(**{
            'video.frame_seq': 918275, 'video.captured_at': '2026-11-14T06:10:09.000Z'}))
        refusal = self.refusal()
        self.assertEqual(refusal['code'], 'video_stale')

    def test_bad_arguments(self):
        with self.assertRaises(shot.InvalidArguments) as raised:
            self.take(timeout_ms=10 ** 9)
        self.assertEqual(raised.exception.field, 'timeout_ms')


if __name__ == '__main__':
    unittest.main()
