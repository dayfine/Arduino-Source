"""shot: the first frame after the call, through the app's `snapshot` command (Slice 2, fork #131).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import tempfile
import unittest
from pathlib import Path

from pa_lab import lab_reader, shot
from tests.fake_lab import PNG, FakeApp, FakeClock, FakeLab, golden

START = '2026-11-14T06:10:12.400Z'
INSTANCE = 'd8ea6a9d-1eab-4a40-b355-d88072b289a1'
NEW_INSTANCE = '11111111-2222-4333-8444-555555555555'
START_SEQ = 918274  # the stopped_ui golden's


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

    def test_returns_the_frame_the_app_saved(self):
        result = self.take()
        self.assertEqual(result['png'], PNG)
        self.assertEqual(result['instance_id'], INSTANCE)
        # The app picks the request up at seq START_SEQ + 1 and saves the next frame.
        self.assertEqual(result['frame_seq'], START_SEQ + 2)
        self.assertEqual(result['captured_at'], '2026-11-14T06:10:12.500Z')
        self.assertTrue(result['path'].endswith('_frame.png'))
        self.assertEqual(result['call_started_at'], START)

    def test_the_request_is_one_snapshot_line(self):
        self.take(timeout_ms=1500)
        self.assertEqual(len(self.app.requests), 1)
        lines = self.app.requests[0].splitlines()
        self.assertRegex(lines[0], r'^request [0-9a-f-]{36}$')
        self.assertEqual(lines[1:], ['snapshot frame 1500'])

    def test_any_panel_and_controller_state(self):
        cases = [
            dict(name='controller not ready', input={'controller.state': 'not_connected'}),
            dict(name='settings panel', input={'panel.kind': 'other', 'panel.id': None,
                                               'program.state': 'no_program'}),
            dict(name='no frame yet', input={'video.state': 'missing', 'video.frame_seq': None,
                                             'video.captured_at': None}),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.lab.update_state(**case['input'])
                self.assertEqual(self.take()['png'], PNG)

    def test_no_newer_frame_is_the_apps_timeout(self):
        self.app.frames = False
        self.assertEqual(self.refusal(), golden('refusal', 'shot_timeout')['refusal'])
        self.assertEqual(len(self.app.requests), 1)

    def test_unavailable_reasons(self):
        cases = [
            dict(name='no video', input='no_video', expectation=('video_stale', 'no_video')),
            dict(name='panel changed', input='panel_changed', expectation=('app_unavailable', 'panel_changed')),
            dict(name='app closing', input='app_closing', expectation=('app_unavailable', 'app_closing')),
            dict(name='save failed', input='save_failed', expectation=('app_unavailable', 'save_failed')),
            dict(name='unknown reason', input='gremlins', expectation=('app_unavailable', 'unknown')),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.app.unavailable = case['input']
                refusal = self.refusal()
                self.assertEqual((refusal['code'], refusal['evidence']['reason']), case['expectation'])

    def test_a_reply_from_another_instance_is_refused(self):
        cases = [
            dict(name='saved', input={}),
            dict(name='timeout', input={'frames': False}),
            dict(name='unavailable', input={'unavailable': 'panel_changed'}),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.app.reply_instance = NEW_INSTANCE
                for knob, value in case['input'].items():
                    setattr(self.app, knob, value)
                refusal = self.refusal()
                self.assertEqual(refusal['code'], 'app_unavailable')
                self.assertEqual(refusal['evidence'], {'reason': 'instance_replaced', 'instance_id': INSTANCE,
                                                       'new_instance_id': NEW_INSTANCE})

    def test_only_the_expected_path_is_opened(self):
        with tempfile.TemporaryDirectory(prefix='pa-lab-decoy-') as decoy_dir:
            self.app.reply_dir = decoy_dir
            # Whatever file the journal names, the server reads the one it asked for in outbox/.
            original = self.app.continue_snapshot

            def plant_decoy(pending):
                (Path(decoy_dir) / '{}_frame.png'.format(pending['name'])).write_bytes(b'decoy')
                original(pending)
            self.app.continue_snapshot = plant_decoy
            result = self.take()
        self.assertEqual(result['png'], PNG)
        self.assertEqual(Path(result['path']).parent, self.lab.dir / 'outbox')

    def test_busy_app_times_out_and_the_request_is_withdrawn(self):
        self.app.serve = False
        refusal = self.refusal(timeout_ms=1000)
        self.assertEqual(refusal['code'], 'timeout')
        self.assertIn('did not answer', refusal['message'])
        self.assertEqual(list((self.lab.dir / 'inbox').glob('*.txt')), [])

    def test_app_failures(self):
        cases = [
            dict(name='line error', input=dict(journal_error='unknown command: snapshot'),
                 expectation=dict(code='app_unavailable', contains='unknown command: snapshot')),
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

    def test_refused_before_asking_the_app(self):
        cases = [
            dict(name='black screen', input={'video.signal': 'black'}, expectation='video_bad_signal'),
            dict(name='future schema', input={'schema': 2}, expectation='unsupported_schema'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                self.setUp()
                self.lab.update_state(**case['input'])
                self.assertEqual(self.refusal()['code'], case['expectation'])
                self.assertEqual(self.app.requests, [])

    def test_saved_frame_older_than_max_age_is_stale(self):
        self.app.frames = False
        self.clock.on_sleep.append(lambda: self.lab.update_state(**{
            'video.frame_seq': START_SEQ + 1, 'video.captured_at': '2026-11-14T06:10:09.000Z'}))
        refusal = self.refusal()
        self.assertEqual(refusal['code'], 'video_stale')
        self.assertEqual(refusal['evidence']['frame_seq'], START_SEQ + 1)

    def test_bad_arguments(self):
        with self.assertRaises(shot.InvalidArguments) as raised:
            self.take(timeout_ms=10 ** 9)
        self.assertEqual(raised.exception.field, 'timeout_ms')


if __name__ == '__main__':
    unittest.main()
