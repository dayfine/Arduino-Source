"""Replay production batch/attempt control flow; substitute only its external services.

No copied retry algorithm: compile the current source function bodies and real tracker.
Navigation, OCR, quest selection, stats and hardware are simulated. This does not
validate OCR, navigation, or paid reroll selection (covered separately by M5).
"""
import os
from pathlib import Path
import re
import shlex
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FARMING = ROOT / 'SerialPrograms/Source/VideoGames/PokemonSV/Programs/Farming'


def replay_source():
    farmer = (FARMING / 'PokemonSV_BBQSoloFarmer.cpp').read_text()
    start = farmer.index('    std::vector<BBQuests> quest_list;')
    end = farmer.index('\n    env.update_stats();\n\n    if (BBQ_OPTIONS.FIX_TIME_WHEN_DONE)', start)
    loop = farmer[start:end]
    quests = (FARMING / 'PokemonSV_BlueberryQuests.cpp').read_text()
    start = quests.index('bool process_and_do_quest(')
    attempt = quests[start:quests.index('\nvoid quest_make_tm(', start)]
    options = (ROOT / 'SerialPrograms/Source/VideoGames/PokemonSV/Options/PokemonSV_BBQOption.h').read_text()
    start = options.index('enum class BBQuests{')
    enums = options[start:options.index('//Quest exclusion table', start)]
    routes = set(re.findall(r'\b(quest_\w+)\(', attempt))
    implementations = {
        'quest_sneak_up': 'Q::sneak_up',
        'quest_make_tm': 'Q::make_tm',
        'quest_travel_500': 'Q::travel_500',
    }
    route_stubs = '\n'.join(
        'template<class... T> void ' + name + '(T&&...){ route(' +
        implementations.get(name, 'Q::UnableToDetect') + '); }'
        for name in sorted(routes)
    )
    return (STUBS.replace('ENUMS', enums).replace('ROUTES', route_stubs) + attempt +
            '\nvoid run_farmer(Env& env, Context& context){\n'
            'Stats& stats=env.stats;\n' + loop + '\n}\n}\n' + MAIN)


STUBS = r'''
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "VideoGames/PokemonSV/Programs/Farming/PokemonSV_BBQQuestRetries.h"
namespace PokemonAutomation::NintendoSwitch::PokemonSV{
using namespace std::chrono_literals;
ENUMS
using Q=BBQuests;
struct ProgramFinishedException{};
constexpr int COLOR_RED=1, BUTTON_A=1, BUTTON_HOME=2;
struct ErrorReportMode{static constexpr int SEND_ERROR_REPORT=0;};
struct Context{void wait_for_all_requests(){}};
using ProControllerContext=Context;
struct Console{template<class... T>void log(const std::string& text,T...){
    if(text.find("exhausted")!=std::string::npos) exhausted_logs++;
} static inline unsigned exhausted_logs=0;};
using ConsoleHandle=Console;
struct OperationFailedExceptionWithScreenshot{
    [[noreturn]] static void fire(int,const char*,Console&){throw std::runtime_error("unexpected route");}
};
struct Stats{unsigned questsCompleted=0,saves=0;};
struct Env{
    Console console;Stats stats;
    int program_info(){return 0;}
    template<class... T>void log(const std::string& s,T... args){console.log(s,args...);}
    void update_stats(){}
};
using ProgramEnvironment=Env;
struct BBQOption{
    enum class OOEggs{Stop,KeepGoing};
    OOEggs OUT_OF_EGGS=OOEggs::KeepGoing;
    unsigned NUM_EGGS=100,NUM_RETRIES=1,NUM_QUESTS=100,SAVE_NUM_QUESTS=0;
};
BBQOption BBQ_OPTIONS;
struct Step{Q quest;bool completes;std::vector<Q> next;};
std::vector<Q> board,dispatched;
std::vector<Step> steps;
unsigned step_index=0,reads=0,batches=0,paid_rerolls=0;
void route(Q quest){
    if(step_index>=steps.size()||steps[step_index].quest!=quest)
        throw std::runtime_error("unexpected dispatch / third attempt");
    dispatched.push_back(quest);
    const auto& step=steps[step_index++];
    if(step.completes)board=step.next;
}
ROUTES
std::vector<Q> read_quests(int,Console&,Context&,const BBQOption&){
    if(++reads>64)throw std::runtime_error("replay did not terminate");
    return board;
}
std::vector<Q> process_quest_list(int,Console&,Context&,const BBQOption&,const std::vector<Q>& input,std::uint8_t){
    ++batches;
    std::vector<Q> result;
    for(auto q:input)if(q==Q::sneak_up||q==Q::make_tm||q==Q::travel_500)result.push_back(q);
    // Simulate the selector's paid fallback so accidental entry is observable.
    if(result.empty()){++paid_rerolls;throw std::runtime_error("unexpected paid fallback");}
    return result;
}
struct ConsoleSettings{
    std::chrono::milliseconds SETTINGS_TO_HOME_DELAY0{0};
    static ConsoleSettings& instance(){static ConsoleSettings settings;return settings;}
};
constexpr int NOTIFICATION_STATUS_UPDATE=0;
template<class... T>void pbf_wait(T&&...){}
template<class... T>void go_home(T&&...){}
template<class... T>void home_to_date_time(T&&...){}
template<class... T>void pbf_press_button(T&&...){}
template<class... T>void resume_game_from_home(T&&...){}
template<class... T>void save_game_from_overworld(T&&...){}
template<class... T>void reset_game(T&&...){}
template<class... T>void send_program_status_notification(T&&...){}
'''

MAIN = r'''
using namespace PokemonAutomation::NintendoSwitch::PokemonSV;
int main(int argc,char**argv){
    if(argc!=2)return 2;
    const std::string name=argv[1];
    const Q a=Q::sneak_up,b=Q::make_tm,c=Q::travel_500,u=Q::pickup_10;
    board={a,b,u};
    auto after_b=std::vector<Q>{a,c,u};
    if(name=="reordered")after_b={u,c,a};
    if(name=="uncertain_absence")after_b={c,u};
    if(name=="complete_absence")after_b={c,u,Q::photo_flying,Q::catch_flying};
    if(name=="unknown_fourth")after_b={c,u,Q::photo_flying,Q::UnableToDetect};
    if(name=="zero_retries")BBQ_OPTIONS.NUM_RETRIES=0;
    steps={{a,false,{}}};
    if(BBQ_OPTIONS.NUM_RETRIES)steps.push_back({a,false,{}});
    steps.push_back({b,true,after_b});
    steps.push_back({c,true,{a,u}});
    if(name=="complete_absence"){
        steps.push_back({a,false,{}});steps.push_back({a,false,{}});
    }
    Env env;Context ctx;bool stopped=false;
    try{run_farmer(env,ctx);}catch(const ProgramFinishedException&){stopped=true;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
    unsigned attempts_a=0;
    for(auto q:dispatched)if(q==a)attempts_a++;
    std::cout<<stopped<<' '<<attempts_a<<' '<<env.stats.questsCompleted<<' '
        <<batches<<' '<<paid_rerolls<<' '<<step_index<<' '<<Console::exhausted_logs;
}
'''


class RetryDispatchTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp = tempfile.TemporaryDirectory(prefix='bbq-retry-dispatch-')
        cls.addClassCleanup(cls.temp.cleanup)
        cls.source = replay_source()
        cls.exe = cls.compile(cls.source, 'replay')

    @classmethod
    def compile(cls, source, name):
        cpp = Path(cls.temp.name) / (name + '.cpp')
        cpp.write_text(source)
        exe = cpp.with_suffix('')
        subprocess.run(shlex.split(os.environ.get('CXX', 'c++')) + [
            '-std=c++23', '-Wall', '-Wextra', '-Werror',
            '-I' + str(ROOT / 'SerialPrograms/Source'), str(cpp), '-o', str(exe),
        ], check=True)
        return exe

    def check_replay(self, name, expected):
        result = subprocess.run([str(self.exe), name], check=True, capture_output=True, text=True)
        # stopped, A attempts, other completions, batches, charges, dispatched steps, exhausted logs
        self.assertEqual(tuple(map(int, result.stdout.split())), expected)

    def test_failed_quest_does_not_restart_after_other_quests_complete(self):
        self.check_replay('persistent', (1, 2, 2, 3, 0, 4, 2))

    def test_reordered_board_preserves_exhausted_budget(self):
        self.check_replay('reordered', (1, 2, 2, 3, 0, 4, 2))

    def test_partial_board_absence_does_not_refresh_budget(self):
        self.check_replay('uncertain_absence', (1, 2, 2, 3, 0, 4, 2))

    def test_unknown_fourth_slot_does_not_refresh_budget(self):
        self.check_replay('unknown_fourth', (1, 2, 2, 3, 0, 4, 2))

    def test_complete_observed_removal_allows_new_quest_budget(self):
        self.check_replay('complete_absence', (1, 4, 2, 4, 0, 6, 3))

    def test_zero_retries_allows_one_attempt_total(self):
        self.check_replay('zero_retries', (1, 1, 2, 3, 0, 3, 2))

    def test_replay_detects_disconnected_session_state(self):
        # Mutation control: the pre-M4 per-call state must fail this replay.
        needle = 'eggs_hatched, &retry_state)'
        self.assertIn(needle, self.source)
        mutated = self.source.replace(needle, 'eggs_hatched, nullptr)')
        exe = self.compile(mutated, 'disconnected-state')
        result = subprocess.run([str(exe), 'persistent'], capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        self.assertIn('unexpected dispatch / third attempt', result.stderr)


if __name__ == '__main__':
    unittest.main()
