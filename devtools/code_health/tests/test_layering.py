import unittest
from pathlib import Path

from devtools.code_health.config import GAME_MODULES, PLATFORM_MODULES, SOURCE_ROOT
from devtools.code_health.layering import check


REPO = Path(__file__).resolve().parents[3]
SHARED = SOURCE_ROOT + 'CommonTools/Async/Example.cpp'


class LayeringTests(unittest.TestCase):
    def test_added_game_includes_fail_in_shared_code(self):
        for include in ('Pokemon/Inference/Pokemon_NameReader.h',
                        'VideoGames/PokemonSV/PokemonSV_Settings.h',
                        'PokemonBDSP/PokemonBDSP_Settings.h',
                        '../../VideoGames/PokemonFRLG/PokemonFRLG_Settings.h'):
            with self.subTest(include=include):
                failures = check(SHARED, f'#include "{include}"\n', '')
                self.assertEqual(1, len(failures))
                self.assertIn(f'{SHARED}:1: layering', str(failures[0]))

    def test_platform_includes_and_game_code_pass(self):
        self.assertEqual([], check(SHARED, '#include "NintendoSwitch/Controllers/X.h"\n'
                                           '#include "Controllers/Controller.h"\n#include <vector>\n', ''))
        game = SOURCE_ROOT + 'VideoGames/PokemonSV/Programs/X.cpp'
        self.assertEqual([], check(game, '#include "Pokemon/Pokemon_Strings.h"\n', ''))

    def test_existing_and_commented_includes_pass(self):
        existing = '#include "Pokemon/Inference/Pokemon_NameReader.h"\r\n'
        self.assertEqual([], check(SHARED, existing + 'int x;\r\n', existing))
        self.assertEqual([], check(SHARED, '//#include "Pokemon/X.h"\n', ''))

    def test_root_common_and_relative_paths(self):
        source = '#include "../SerialPrograms/Source/VideoGames/PokemonLA/X.h"\n'
        self.assertEqual(1, len(check('Common/Cpp/X.cpp', source, None)))

    def test_every_top_level_source_directory_is_classified(self):
        root = REPO / SOURCE_ROOT
        if not root.is_dir():
            self.skipTest('SerialPrograms/Source is not checked out')
        directories = {p.name for p in root.iterdir() if p.is_dir()}
        self.assertEqual(set(), directories - set(GAME_MODULES) - set(PLATFORM_MODULES))
        self.assertEqual(set(), set(GAME_MODULES) & set(PLATFORM_MODULES))


if __name__ == '__main__':
    unittest.main()
