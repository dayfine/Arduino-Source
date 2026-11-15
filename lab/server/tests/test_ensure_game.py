"""ensure_game: read-only verification of the game and screen observations (Slice 2, fork #131).

Run from the repo root: python3 -m unittest discover -s lab/server/tests -t lab/server -v
"""

import unittest

from pa_lab import lab_reader, tools
from tests.fake_lab import FakeClock, FakeLab, golden

BDSP = {'value': 'PokemonBDSP', 'observed_at': '2026-11-14T07:02:41.000Z', 'detector': 'bdsp_overworld'}
OVERWORLD = {'value': 'overworld', 'observed_at': '2026-11-14T07:02:41.000Z', 'detector': 'bdsp_overworld'}
UNKNOWN = {'value': 'unknown', 'observed_at': None}


class EnsureGameTests(unittest.TestCase):
    def test_verdicts(self):
        cases = [
            dict(name='detectors not there yet',
                 input=dict(game=UNKNOWN, screen=UNKNOWN, args={'expected': 'PokemonBDSP'}),
                 expectation=dict(verified=False, reason='game_unknown', age_ms=None)),
            dict(name='game matches, no screen asked',
                 input=dict(game=BDSP, screen=UNKNOWN, args={'expected': 'PokemonBDSP'}),
                 expectation=dict(verified=True, reason=None, age_ms=1000)),
            dict(name='another game',
                 input=dict(game=BDSP, screen=UNKNOWN, args={'expected': 'PokemonFRLG'}),
                 expectation=dict(verified=False, reason='game_mismatch', age_ms=1000)),
            dict(name='screen allowed',
                 input=dict(game=BDSP, screen=OVERWORLD,
                            args={'expected': 'PokemonBDSP', 'allowed_screens': ['overworld', 'menu']}),
                 expectation=dict(verified=True, reason=None, age_ms=1000)),
            dict(name='screen unknown',
                 input=dict(game=BDSP, screen=UNKNOWN,
                            args={'expected': 'PokemonBDSP', 'allowed_screens': ['overworld']}),
                 expectation=dict(verified=False, reason='screen_unknown', age_ms=1000)),
            dict(name='screen not allowed',
                 input=dict(game=BDSP, screen=OVERWORLD,
                            args={'expected': 'PokemonBDSP', 'allowed_screens': ['battle']}),
                 expectation=dict(verified=False, reason='screen_not_allowed', age_ms=1000)),
            dict(name='observation too old',
                 input=dict(game=BDSP, screen=UNKNOWN, args={'expected': 'PokemonBDSP', 'max_age_ms': 500}),
                 expectation=dict(verified=False, reason='observation_stale', age_ms=1000)),
        ]
        for case in cases:
            with self.subTest(case['name']):
                lab = FakeLab(self)
                lab.state = golden('app_state', 'running_lab')
                lab.update_state(**{'observation.game': case['input']['game'],
                                    'observation.screen': case['input']['screen']})
                console = lab_reader.ConsoleReader('B', lab.dir, profile='lab-B',
                                                   now=FakeClock('2026-11-14T07:02:42.000Z').now,
                                                   pid_alive=lambda pid: True)
                result = tools.ensure_game(console, case['input']['args'])
                want = case['expectation']
                self.assertEqual((result['verified'], result['reason'], result['game_age_ms']),
                                 (want['verified'], want['reason'], want['age_ms']))
                self.assertEqual(result['game'], case['input']['game'])

    def test_bad_arguments_name_the_field(self):
        cases = [
            dict(name='no expected', input={}, expectation='expected'),
            dict(name='expected not an id', input={'expected': 'Pokemon FRLG'}, expectation='expected'),
            dict(name='unknown argument', input={'expected': 'PokemonFRLG', 'navigate': True},
                 expectation='navigate'),
            dict(name='screens not a list', input={'expected': 'PokemonFRLG', 'allowed_screens': 'menu'},
                 expectation='allowed_screens'),
        ]
        lab = FakeLab(self)
        lab.write_state(golden('app_state', 'running_lab'))
        console = lab_reader.ConsoleReader('B', lab.dir, now=FakeClock('2026-11-14T07:02:42.000Z').now,
                                           pid_alive=lambda pid: True)
        for case in cases:
            with self.subTest(case['name']):
                with self.assertRaises(tools.InvalidArguments) as raised:
                    tools.ensure_game(console, case['input'])
                self.assertEqual(raised.exception.field, case['expectation'])


if __name__ == '__main__':
    unittest.main()
