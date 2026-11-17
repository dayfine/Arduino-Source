import os
import unittest
from pathlib import Path

from devtools.code_health.config import GAME_MODULES, PLATFORM_MODULES, SOURCE_ROOT
from devtools.code_health.layering import check


# The code checkout to classify: CODE_HEALTH_REPO (CI sets it), else this tree.
REPO = Path(os.environ.get('CODE_HEALTH_REPO') or Path(__file__).resolve().parents[3])
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
            self.skipTest('SerialPrograms/Source is not checked out; set CODE_HEALTH_REPO')
        directories = {p.name for p in root.iterdir() if p.is_dir()}
        self.assertEqual(set(), directories - set(GAME_MODULES) - set(PLATFORM_MODULES))
        self.assertEqual(set(), set(GAME_MODULES) & set(PLATFORM_MODULES))

    def test_added_heavy_includes_fail_in_pure_modules(self):
        pure = SOURCE_ROOT + 'VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCaptureLogic.cpp'
        for include in ('NintendoSwitch/NintendoSwitch_ConsoleHandle.h',
                        'CommonFramework/ImageTypes/ImageViewRGB32.h',
                        'VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_ItemListReader.h',
                        'VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h',
                        'QWidget'):
            with self.subTest(include=include):
                failures = check(pure, f'#include "{include}"\n', '')
                self.assertEqual(1, len(failures))
                self.assertIn(f'{pure}:1: layering', str(failures[0]))

    def test_heavy_includes_allowed_in_non_pure_and_when_preexisting(self):
        hw = SOURCE_ROOT + 'VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryCapture.cpp'
        self.assertEqual([], check(hw, '#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"\n', ''))
        pure = SOURCE_ROOT + 'VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStorySupplies.h'
        existing = '#include "CommonFramework/ImageTypes/ImageViewRGB32.h"\n'
        self.assertEqual([], check(pure, existing + 'struct S{};\n', existing))


if __name__ == '__main__':
    unittest.main()
