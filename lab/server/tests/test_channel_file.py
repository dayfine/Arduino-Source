"""The version-1 file channel (`pa_lab.channel_file`, `contracts/v1/channel.md`) on a temp `Lab/` folder.

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import os
import unittest

from pa_lab.channel import (AppError, DescribeProgram, ErrorKind, GameOpen, Journal, Snapshot, Start, Stop,
                            await_answer)
from pa_lab.channel_file import FileChannel, parse_journal
from tests.fake_lab import FakeClock, FakeLab

REQUEST_ID = '6f1a2b3c-4d5e-4f60-8a71-9b0c1d2e3f40'


class FileChannelTest(unittest.TestCase):
    def setUp(self):
        self.lab = FakeLab(self)
        self.channel = FileChannel(self.lab.dir)

    def inbox(self):
        return sorted(p.name for p in (self.lab.dir / 'inbox').iterdir())

    def take(self, name):
        """What LabChannel does first: move the request from inbox/ to done/."""
        os.replace(self.lab.dir / 'inbox' / (name + '.txt'), self.lab.dir / 'done' / (name + '.txt'))

    def write_journal(self, name, text):
        (self.lab.dir / 'outbox' / (name + '.out')).write_text(text, encoding='utf-8')

    def test_a_request_is_one_inbox_file_of_its_id_and_command_lines(self):
        cases = [
            dict(name='start', input=(Start(controller_wait_s=10),), expectation='start 10'),
            dict(name='stop', input=(Stop(),), expectation='stop'),
            dict(name='snapshot', input=(Snapshot(label='frame', timeout_ms=5000),),
                 expectation='snapshot frame 5000'),
            dict(name='describe_program', input=(DescribeProgram(),), expectation='describe_program'),
            dict(name='game_open', input=(GameOpen(),), expectation='game_open'),
        ]
        for case in cases:
            with self.subTest(case['name']):
                name = self.channel.request_name(case['name'], REQUEST_ID)
                self.channel.send(name, REQUEST_ID, case['input'])
                text = (self.lab.dir / 'inbox' / (name + '.txt')).read_text(encoding='utf-8')
                self.assertEqual(text, 'request {}\n{}\n'.format(REQUEST_ID, case['expectation']))
                self.assertEqual(list((self.lab.dir / 'outbox').iterdir()), [])  # no .part left behind
                self.assertTrue(self.channel.withdraw(name))

    def test_a_failed_send_raises_and_leaves_no_part_file(self):
        (self.lab.dir / 'inbox').rmdir()  # the rename into the inbox fails
        name = self.channel.request_name('stop', REQUEST_ID)
        with self.assertRaises(OSError):
            self.channel.send(name, REQUEST_ID, (Stop(),))
        self.assertEqual(list((self.lab.dir / 'outbox').iterdir()), [])

    def test_a_unique_name_differs_per_attempt_of_one_request_id(self):
        names = {self.channel.request_name('run_program', REQUEST_ID, unique=True) for _ in range(3)}
        self.assertEqual(len(names), 3)

    def test_a_journal_is_an_answer_only_once_it_ends(self):
        name = self.channel.request_name('game_open', REQUEST_ID)
        self.channel.send(name, REQUEST_ID, (GameOpen(),))
        self.assertIsNone(self.channel.answer(name))
        self.write_journal(name, 'game_open yes\n')
        self.assertIsNone(self.channel.answer(name))
        self.write_journal(name, 'game_open yes\nok 2 game_open @0-3ms\nend\n')
        journal = self.channel.answer(name)
        self.assertTrue(journal.ok(GameOpen))
        self.assertEqual(journal.printed, ('game_open yes',))

    def test_taken_follows_the_request_out_of_the_inbox(self):
        name = self.channel.request_name('run_program', REQUEST_ID, unique=True)
        self.channel.send(name, REQUEST_ID, (Start(controller_wait_s=10),))
        self.assertFalse(self.channel.taken(name))
        self.take(name)
        self.assertTrue(self.channel.taken(name))
        self.assertFalse(self.channel.withdraw(name))
        self.write_journal(name, 'ok 2 start 10 @0-41ms\nend\n')
        (self.lab.dir / 'done' / (name + '.txt')).unlink()  # pruned: the journal still shows it taken
        self.assertTrue(self.channel.taken(name))

    def test_an_unanswered_request_is_withdrawn_at_the_deadline(self):
        clock = FakeClock('2026-11-14T06:00:00.000Z')
        name = self.channel.request_name('shot', REQUEST_ID)
        self.channel.send(name, REQUEST_ID, (Snapshot(label='frame', timeout_ms=100),))
        self.assertIsNone(await_answer(self.channel, name, clock.now, clock.sleep, clock.now() + 1, poll_s=0.1))
        self.assertEqual(self.inbox(), [])

    def test_state_and_save_are_none_until_the_app_writes_them(self):
        self.assertIsNone(self.channel.read_state(limit=10))
        self.assertIsNone(self.channel.read_save(limit=10))
        self.lab.write_raw(self.lab.state_path, '{"schema": 1}')
        self.assertEqual(self.channel.read_state(limit=100), b'{"schema": 1}')

    def test_a_snapshot_is_saved_beside_its_journal(self):
        self.assertEqual(self.channel.snapshot_path('palab_shot_1', 'frame'),
                         str(self.lab.dir / 'outbox' / 'palab_shot_1_frame.png'))


class ParseJournalTest(unittest.TestCase):
    def test_error_lines_become_kinds(self):
        cases = [
            dict(name='owner lock', input='owned by 7f3e',
                 expectation=AppError(ErrorKind.OWNER_CLAIMED, 'owned by 7f3e', command_index=0)),
            dict(name='program state', input='Program is RUNNING.',
                 expectation=AppError(ErrorKind.PROGRAM_STATE, 'Program is RUNNING.', program_state='RUNNING',
                                      command_index=0)),
            dict(name='not a program', input='the selected panel is not a program',
                 expectation=AppError(ErrorKind.PANEL_NOT_PROGRAM, 'the selected panel is not a program',
                                      command_index=0)),
            dict(name='controller', input='controller not connected after 10 s',
                 expectation=AppError(ErrorKind.CONTROLLER_NOT_READY, 'controller not connected after 10 s',
                                      command_index=0)),
            dict(name='anything else', input='unknown command',
                 expectation=AppError(ErrorKind.OTHER, 'unknown command', command_index=0)),
        ]
        for case in cases:
            with self.subTest(case['name']):
                journal = parse_journal(('error 2 (start 10): ' + case['input'], 'end'))
                self.assertEqual(journal.error, case['expectation'])
                self.assertFalse(journal.ok(Start))

    def test_ok_lines_name_the_commands_that_ran_and_the_rest_is_printed(self):
        lines = ('snapshot saved instance=x frame_seq=2 captured_at=t after=1 path=p',
                 'ok 2 snapshot frame 5000 @0-80ms', 'end')
        self.assertEqual(parse_journal(lines), Journal(lines=lines, succeeded=frozenset({Snapshot}),
                                                       printed=(lines[0],)))

    def test_only_the_first_error_is_kept(self):
        journal = parse_journal(('ok 2 stop @0-1ms', 'error 3 (start 10): Program is STOPPING.',
                                 'error 4 (stop): owned by x', 'end'))
        self.assertEqual((journal.error.kind, journal.error.command_index, journal.ok(Stop)),
                         (ErrorKind.PROGRAM_STATE, 1, True))


if __name__ == '__main__':
    unittest.main()
