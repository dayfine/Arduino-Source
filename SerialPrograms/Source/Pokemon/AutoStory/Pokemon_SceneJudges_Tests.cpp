/*  Scene Judges Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The judgements over screen events: warp seen and talk-over.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_SceneJudges.h"
#include "Pokemon_SceneJudges_Tests.h"
#include "Pokemon_TestExpect.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;


UnitTestResult test_warp_seen(){
    WarpSeen seen = note_warp({}, {ScreenEvent::DIALOG_BOX}, "Route 203");
    if (seen.any()){
        return "a dialog box with no banner event counted as a warp";
    }
    seen = note_warp(seen, {ScreenEvent::FADE_BLACK}, "");
    seen = note_warp(seen, {ScreenEvent::FADE_WHITE, ScreenEvent::LOCATION_BANNER}, "Route 203");
    seen = note_warp(seen, {ScreenEvent::LOCATION_BANNER}, "Oreburgh Gate");
    return expect_equal("describe(WarpSeen)", describe(seen), "black fade, banner \"Route 203\"");
}


//  A scripted scene: a look per beat, a second apart. Returns the beat
//  TalkOver first says `want` at, or -1.
int first_beat(const std::vector<ScreenEvents>& beats, SceneMove want, TalkOver& judge){
    const Milliseconds every = 1000ms;
    WallClock now = WallClock();
    for (size_t c = 0; c < beats.size(); c++){
        if (judge.look(beats[c], now) == want){
            return (int)c;
        }
        now += every;
    }
    return -1;
}

const ScreenEvents QUIET{ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL};
const ScreenEvents CALM{ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents BOX{ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL};
//  A black frame as BDSP's eyes see it: no box, so "overworld", and still.
const ScreenEvents FADED{ScreenEvent::FADE_BLACK, ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::SCREEN_STILL};
const ScreenEvents CALM_FADED{ScreenEvent::FADE_BLACK, ScreenEvent::PLAYER_CAN_MOVE};

std::vector<ScreenEvents> repeat(const ScreenEvents& look, size_t count){
    return std::vector<ScreenEvents>(count, look);
}
std::vector<ScreenEvents> concat(std::vector<ScreenEvents> a, const std::vector<ScreenEvents>& b){
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

UnitTestResult test_talk_over(){
    struct Row{
        std::string name;
        TalkOverRule rule;
        std::vector<ScreenEvents> beats;
        SceneMove want;
        int beat;
    };
    const std::vector<Row> rows = {
        {"5 s of quiet ends it", {}, repeat(QUIET, 8), SceneMove::DONE, 5},
        {"a box restarts the quiet", {}, concat(concat(repeat(QUIET, 3), {BOX}), repeat(QUIET, 8)), SceneMove::DONE, 9},
        {"a box is answered", {}, {BOX}, SceneMove::NEXT, 0},
        {"no box is a B press", {}, {QUIET}, SceneMove::B, 0},
        {"15 s calm but moving ends it", {}, repeat(CALM, 20), SceneMove::DONE, 15},
        {"a warp rule waits for the fade", {.warp = true},
            concat(concat(repeat(QUIET, 8), {ScreenEvents{ScreenEvent::FADE_BLACK}}), repeat(QUIET, 8)),
            SceneMove::DONE, 14},
        {"a banner is the warp too", {.warp = true},
            concat({ScreenEvents{ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE}}, repeat(QUIET, 8)),
            SceneMove::DONE, 6},
        {"no quiet ends a talk before a battle", {.end_on_quiet = false}, repeat(QUIET, 30), SceneMove::DONE, -1},
        {"a battle", {.end_on_quiet = false}, concat(repeat(QUIET, 6), {ScreenEvents{ScreenEvent::BATTLE_STARTED}}), SceneMove::BATTLE, 6},
        //  Quiet, then the scene's closing fade: the scene must not end at
        //  the fade's start, or the next step's input lands in the fade.
        {"a closing fade starts the quiet over", {},
            concat(concat(repeat(QUIET, 4), repeat(FADED, 3)), repeat(QUIET, 8)),
            SceneMove::DONE, 12},
        {"a fade still down never ends it", {}, concat(repeat(QUIET, 3), repeat(FADED, 20)), SceneMove::DONE, -1},
        {"a fade is never calm", {}, concat(repeat(CALM, 10), repeat(CALM_FADED, 10)), SceneMove::DONE, -1},
    };
    for (const Row& row : rows){
        TalkOver judge(row.rule);
        const int got = first_beat(row.beats, row.want, judge);
        if (got != row.beat){
            return std::format("{}: expected beat {}, got {} ({})", row.name, row.beat, got, judge.describe());
        }
    }
    return true;
}


struct SceneJudgesCase{
    struct Input{
        std::string name;
        UnitTestResult (*body)();
    };

    struct Expectation{
    };

    Input input;
    Expectation expectation;
};

class Test_SceneJudges : public UnitTest{
public:
    explicit Test_SceneJudges(SceneJudgesCase test_case)
        : UnitTest(std::format("Pokemon::SceneJudges - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    SceneJudgesCase m_case;
};

} // namespace


const SceneJudgesCase SCENE_JUDGES_CASES[] = {
    {
        .input = {"a warp is the first fade and the first banner seen", test_warp_seen},
        .expectation = {},
    },
    {
        .input = {"talk-over from scripted events", test_talk_over},
        .expectation = {},
    },
};

void add_tests_SceneJudges(UnitTestDatabase& database){
    for (const auto& item : SCENE_JUDGES_CASES){
        database.add<Test_SceneJudges>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
