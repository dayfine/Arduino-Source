"""Replay the production BBQ entry/dispatch blocks with controller-free C++ stubs.

Run: python3 -B -m unittest discover -s tests/pokemon_sv -v
This checks command dispatch and the two confirmation sources, not video/audio
inference or navigation. A full build and Switch trials cover those boundaries.
"""

import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "SerialPrograms/Source/VideoGames/PokemonSV/Programs/Farming/PokemonSV_BlueberryQuests.cpp"


def production_blocks(name):
    source = SOURCE.read_text()
    start = source.index("void " + name + "(")
    end = source.index("\nvoid ", start + 1)
    function = source[start:end]
    entry_start = function.index("            NormalBattleMenuWatcher battle_menu(COLOR_YELLOW);")
    entry_end = function.index("\n        },", entry_start)
    dispatch_start = function.index("    encounter_watcher.throw_if_no_sound();")
    dispatch_end = function.index("    return_to_plaza(info, console, context);", dispatch_start)
    dispatch_end += len("    return_to_plaza(info, console, context);")
    return function[entry_start:entry_end], function[dispatch_start:dispatch_end]


STUBS = r'''
#include <chrono>
#include <initializer_list>
#include <iostream>
#include <string>
using namespace std::chrono_literals;

struct Trace{
    int battle = 0, capture = 0, recovery = 0, stop = 0, audio_error = 0;
    int inner_result = -1, tera = -1;
} trace;
struct ProgramFinishedException{};
struct AudioFailure{};
struct ProgramInfo{};
struct Context{};
struct Console{
    void log(const char*){}
};
struct EncounterWatcher{
    bool shiny, missing_audio;
    void throw_if_no_sound() const{
        if (missing_audio) throw AudioFailure();
    }
    bool shiny_screenshot() const{ return shiny; }
};
constexpr int COLOR_YELLOW = 1, BUTTON_CAPTURE = 2, BUTTON_PLUS = 3;
struct NormalBattleMenuWatcher{
    explicit NormalBattleMenuWatcher(int){}
};
int wait_until(Console&, Context&, std::chrono::seconds,
    std::initializer_list<NormalBattleMenuWatcher>){
    return trace.inner_result;
}
template <class... Args>
void pbf_press_button(Context&, int button, Args&&...){
    if (button == BUTTON_CAPTURE) trace.capture++;
}
void wild_battle_tera(const ProgramInfo&, Console&, Context&, bool& tera){
    trace.battle++;
    trace.tera = tera;
}
void return_to_plaza(const ProgramInfo&, Console&, Context&){ trace.recovery++; }
'''


class BBQBattleEntryTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temporary = tempfile.TemporaryDirectory(prefix="bbq-m3-")
        cls.addClassCleanup(cls.temporary.cleanup)
        folder = Path(cls.temporary.name)
        cpp = STUBS
        for label, name in (("self", "quest_tera_self_defeat"), ("wild", "quest_wild_tera")):
            entry, dispatch = production_blocks(name)
            cpp += "\nvoid replay_" + label + "(int ret, bool run_inner, bool shiny, bool missing_audio){\n"
            cpp += "    ProgramInfo info; Console console; Context context;\n"
            cpp += "    EncounterWatcher encounter_watcher{shiny, missing_audio};\n"
            cpp += "    bool battle_menu_detected = false;\n"
            cpp += "    if (run_inner){\n" + entry + "\n    }\n" + dispatch + "\n}\n"
        cpp += r'''
int main(int argc, char** argv){
    if (argc != 7) return 2;
    trace.inner_result = std::stoi(argv[3]);
    try{
        auto replay = std::string(argv[1]) == "self" ? replay_self : replay_wild;
        replay(std::stoi(argv[2]), std::stoi(argv[4]), std::stoi(argv[5]), std::stoi(argv[6]));
    }catch (const ProgramFinishedException&){ trace.stop++; }
    catch (const AudioFailure&){ trace.audio_error++; }
    std::cout << trace.battle << ' ' << trace.tera << ' ' << trace.capture << ' '
        << trace.recovery << ' ' << trace.stop << ' ' << trace.audio_error;
}
'''
        path = folder / "entry_replay.cpp"
        path.write_text(cpp)
        cls.executable = folder / "entry_replay"
        command = shlex.split(os.environ.get("CXX", "c++"))
        subprocess.run(command + ["-std=c++23", str(path), "-o", str(cls.executable)], check=True)

    def check_entry(self, *, outer=-1, inner=-1, run_inner=True, shiny=False,
                    missing_audio=False, battle=False, stop=False, audio_error=False):
        for route, tera in (("self", 1), ("wild", 0)):
            with self.subTest(route=route):
                result = subprocess.run(
                    [str(self.executable), route, str(outer), str(inner), str(int(run_inner)),
                     str(int(shiny)), str(int(missing_audio))],
                    check=True, capture_output=True, text=True,
                )
                expected = (int(battle), tera if battle else -1, int(stop),
                            int(not stop and not audio_error), int(stop), int(audio_error))
                self.assertEqual(tuple(map(int, result.stdout.split())), expected)

    def test_entry_timeout_skips_battle_and_recovers(self):
        self.check_entry()

    def test_outer_visual_confirmation_runs_battle(self):
        self.check_entry(outer=0, run_inner=False, battle=True)

    def test_inner_confirmation_survives_outer_completion(self):
        self.check_entry(inner=0, battle=True)

    def test_both_confirmations_run_battle_once(self):
        self.check_entry(outer=0, inner=0, battle=True)

    def test_audio_callback_index_is_not_visual_confirmation(self):
        self.check_entry(outer=1)

    def test_unknown_callback_index_is_not_confirmation(self):
        self.check_entry(outer=2)

    def test_shiny_stops_even_without_menu_confirmation(self):
        self.check_entry(shiny=True, stop=True)

    def test_confirmed_shiny_stops_before_battle_or_recovery(self):
        self.check_entry(outer=0, inner=0, shiny=True, stop=True)

    def test_missing_audio_preserves_existing_error(self):
        self.check_entry(outer=0, inner=0, missing_audio=True, audio_error=True)


if __name__ == "__main__":
    unittest.main()
