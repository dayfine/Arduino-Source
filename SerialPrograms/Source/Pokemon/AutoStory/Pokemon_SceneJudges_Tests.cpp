/*  Scene Judges Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The judgements over screen events: warp seen and talk-over.
 *
 */

#include <format>
#include <optional>
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
//  TalkOver first says `want` at, if any.
std::optional<size_t> first_beat(const std::vector<ScreenEvents>& beats, SceneMove want, TalkOver& judge){
    const Milliseconds every = 1000ms;
    WallClock now = WallClock();
    for (size_t c = 0; c < beats.size(); c++){
        if (judge.look(beats[c], now) == want){
            return c;
        }
        now += every;
    }
    return std::nullopt;
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
    //  The judge given `beats`, one look each, from the start.
    struct Row{
        std::string_view name;
        struct Input{
            TalkOverRule rule;
            std::vector<ScreenEvents> beats;
        };
        //  The first beat at which the judge says `move`; none: never.
        struct Expectation{
            SceneMove move;
            std::optional<size_t> beat;
        };
        Input input;
        Expectation expectation;
    };
    using M = SceneMove;
    const ScreenEvents black{ScreenEvent::FADE_BLACK};
    const ScreenEvents banner{ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE};
    const ScreenEvents battle{ScreenEvent::BATTLE_STARTED};
    const std::vector<Row> rows = {
        {.name = "5 s of quiet ends it",
            .input = {.rule = {}, .beats = repeat(QUIET, 8)}, .expectation = {.move = M::DONE, .beat = 5}},
        {.name = "a box restarts the quiet",
            .input = {.rule = {}, .beats = concat(concat(repeat(QUIET, 3), {BOX}), repeat(QUIET, 8))},
            .expectation = {.move = M::DONE, .beat = 9}},
        {.name = "a box is answered", .input = {.rule = {}, .beats = {BOX}}, .expectation = {.move = M::NEXT, .beat = 0}},
        {.name = "no box is a B press", .input = {.rule = {}, .beats = {QUIET}}, .expectation = {.move = M::B, .beat = 0}},
        {.name = "15 s calm but moving ends it",
            .input = {.rule = {}, .beats = repeat(CALM, 20)}, .expectation = {.move = M::DONE, .beat = 15}},
        //  A game whose party bar never shows mid-scene for long, and whose
        //  overworld never stands still, ends a scene on a shorter calm.
        {.name = "a shorter calm rule ends it sooner",
            .input = {.rule = {.calm = 1000ms}, .beats = repeat(CALM, 5)}, .expectation = {.move = M::DONE, .beat = 1}},
        {.name = "a shorter calm rule still waits out a box",
            .input = {.rule = {.calm = 1000ms}, .beats = concat(concat({CALM}, {BOX}), repeat(CALM, 5))},
            .expectation = {.move = M::DONE, .beat = 3}},
        {.name = "a warp rule waits for the fade",
            .input = {.rule = {.warp = true}, .beats = concat(concat(repeat(QUIET, 8), {black}), repeat(QUIET, 8))},
            .expectation = {.move = M::DONE, .beat = 14}},
        {.name = "a banner is the warp too",
            .input = {.rule = {.warp = true}, .beats = concat({banner}, repeat(QUIET, 8))},
            .expectation = {.move = M::DONE, .beat = 6}},
        {.name = "no quiet ends a talk before a battle",
            .input = {.rule = {.end_on_quiet = false}, .beats = repeat(QUIET, 30)},
            .expectation = {.move = M::DONE, .beat = std::nullopt}},
        {.name = "a battle",
            .input = {.rule = {.end_on_quiet = false}, .beats = concat(repeat(QUIET, 6), {battle})},
            .expectation = {.move = M::BATTLE, .beat = 6}},
        //  Quiet, then the scene's closing fade: the scene must not end at
        //  the fade's start, or the next step's input lands in the fade.
        {.name = "a closing fade starts the quiet over",
            .input = {.rule = {}, .beats = concat(concat(repeat(QUIET, 4), repeat(FADED, 3)), repeat(QUIET, 8))},
            .expectation = {.move = M::DONE, .beat = 12}},
        {.name = "a fade still down never ends it",
            .input = {.rule = {}, .beats = concat(repeat(QUIET, 3), repeat(FADED, 20))},
            .expectation = {.move = M::DONE, .beat = std::nullopt}},
        {.name = "a fade is never calm",
            .input = {.rule = {}, .beats = concat(repeat(CALM, 10), repeat(CALM_FADED, 10))},
            .expectation = {.move = M::DONE, .beat = std::nullopt}},
    };
    for (const Row& row : rows){
        TalkOver judge(row.input.rule);
        const std::optional<size_t> got = first_beat(row.input.beats, row.expectation.move, judge);
        if (got != row.expectation.beat){
            const auto show = [](const std::optional<size_t>& beat){
                return beat ? std::to_string(*beat) : std::string("none");
            };
            return std::format("{}: expected beat {}, got {} ({})", row.name, show(row.expectation.beat), show(got), judge.describe());
        }
    }
    return true;
}


//  A named test; it returns its own failure.
struct SceneJudgesCase{
    std::string name;
    UnitTestResult (*body)();
};

class Test_SceneJudges : public UnitTest{
public:
    explicit Test_SceneJudges(SceneJudgesCase test_case)
        : UnitTest(std::format("Pokemon::SceneJudges - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.body();
    }

private:
    SceneJudgesCase m_case;
};

} // namespace


const SceneJudgesCase SCENE_JUDGES_CASES[] = {
    {.name = "a warp is the first fade and the first banner seen", .body = test_warp_seen},
    {.name = "talk-over from scripted events", .body = test_talk_over},
};

void add_tests_SceneJudges(UnitTestDatabase& database){
    for (const auto& item : SCENE_JUDGES_CASES){
        database.add<Test_SceneJudges>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
