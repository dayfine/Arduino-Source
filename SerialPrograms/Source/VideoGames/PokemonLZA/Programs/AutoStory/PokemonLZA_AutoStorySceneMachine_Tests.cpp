/*  PokemonLZA AutoStory - Scene machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scene machine on scripted looks, no frames needed: its timeline's
 *  state for each look of a sequence, and whole runs on a FakeStoryBackend
 *  (their outcome, presses and log).
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStorySceneMachine_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

using Pokemon::Observation;
using Pokemon::ZonePose;


std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

//  A look `at` after the scripts' common start, with `events` and the
//  readings named in `readings`.
Observation<ZonePose> look_at(Milliseconds at, ScreenEvents events = {}, std::vector<std::string_view> readings = {}){
    Observation<ZonePose> look;
    look.when = WallClock::min() + at;
    look.events = events;
    for (std::string_view reading : readings){
        look.readings[std::string(reading)] = 1;
    }
    return look;
}

const ScreenEvents NOTHING{};
const ScreenEvents DIALOG{ScreenEvent::DIALOG_BOX};
const ScreenEvents STILL{ScreenEvent::SCREEN_STILL};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents CHOICE{ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};

//  mash_to_move_hints' config: a minute quiet, blind A after 5 s.
SceneMachineConfig arrival_config(){
    SceneMachineConfig config;
    config.quiet = 60000ms;
    config.blind_press_after = 5000ms;
    return config;
}

//  text_to_choice's config: B through the text, ending at the choice box.
SceneMachineConfig choice_config(){
    SceneMachineConfig config;
    config.stop_at_choice = true;
    config.text_button = BUTTON_B;
    return config;
}

//  story_battle_start's config: A through the talk, A every 2 s on screens
//  with no text, ending on the battle HUD.
SceneMachineConfig battle_config(){
    SceneMachineConfig config;
    config.stop_at_battle = true;
    config.blind_press_after = 0ms;
    return config;
}

//  `count` looks with the mission banner, then one without.
std::vector<Observation<ZonePose>> banner_then_gone(size_t count){
    std::vector<Observation<ZonePose>> looks(count, look_at(0ms, NOTHING, {MISSION_BANNER_READING}));
    looks.push_back(look_at(0ms));
    return looks;
}

//  A quiet time of 8 s after a scene of at least `min_length`, no blind A.
SceneMachineConfig quiet_config(Milliseconds min_length){
    SceneMachineConfig config;
    config.quiet = 8000ms;
    config.min_length = min_length;
    return config;
}


struct TimelineCase{
    std::string name;
    struct Input{
        SceneMachineConfig config;
        std::vector<Observation<ZonePose>> looks;
    };
    Input input;
    struct Expectation{
        //  The state of each look, in order.
        std::vector<SceneScreen> states;
    };
    Expectation expectation;
};

const TimelineCase TIMELINE_CASES[] = {
    {
        .name = "a still frame past the quiet time waits for the minimum length",
        .input = {.config = quiet_config(60000ms), .looks = {
            look_at(0ms), look_at(5000ms, DIALOG), look_at(10000ms, STILL), look_at(30000ms, STILL),
            look_at(59900ms), look_at(60000ms, STILL),
        }},
        .expectation = {.states = {
            SceneScreen::SCENE_MOVING, SceneScreen::DIALOG, SceneScreen::SCENE_STILL, SceneScreen::SCENE_STILL,
            SceneScreen::SCENE_MOVING, SceneScreen::QUIET_END,
        }},
    },
    {
        .name = "a box or a fade starts the quiet time again",
        .input = {.config = quiet_config(0ms), .looks = {
            look_at(0ms, DIALOG), look_at(7900ms, STILL), look_at(7950ms, WHITE), look_at(15000ms, WHITE),
            look_at(20000ms), look_at(22900ms, STILL), look_at(23000ms, STILL),
        }},
        .expectation = {.states = {
            SceneScreen::DIALOG, SceneScreen::SCENE_STILL, SceneScreen::FADE_WHITE, SceneScreen::FADE_WHITE,
            SceneScreen::SCENE_MOVING, SceneScreen::SCENE_STILL, SceneScreen::QUIET_END,
        }},
    },
    {
        .name = "a black fade is busy and waits",
        .input = {.config = quiet_config(0ms), .looks = {
            look_at(0ms, BLACK), look_at(7000ms, BLACK), look_at(14000ms, BLACK), look_at(21900ms),
        }},
        .expectation = {.states = {
            SceneScreen::FADE_BLACK, SceneScreen::FADE_BLACK, SceneScreen::FADE_BLACK, SceneScreen::SCENE_MOVING,
        }},
    },
    {
        .name = "the controls hint held a second ends the scene; a box over it starts the second again",
        .input = {.config = quiet_config(0ms), .looks = {
            look_at(/*at=*/0ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/600ms, DIALOG, {CONTROLS_HINT_READING}),
            look_at(/*at=*/900ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/1800ms, STILL, {CONTROLS_HINT_READING}),
            look_at(/*at=*/9000ms, STILL, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.states = {
            SceneScreen::HINT_SHOWING, SceneScreen::DIALOG, SceneScreen::HINT_SHOWING, SceneScreen::HINT_SHOWING,
            SceneScreen::CONTROLS_HINT,
        }},
    },
    {
        .name = "the controls hint is not held across a look without it",
        .input = {.config = quiet_config(0ms), .looks = {
            look_at(/*at=*/0ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(500ms),
            look_at(/*at=*/1200ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/2200ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.states = {
            SceneScreen::HINT_SHOWING, SceneScreen::SCENE_MOVING, SceneScreen::HINT_SHOWING, SceneScreen::CONTROLS_HINT,
        }},
    },
    {
        .name = "01.01's ad: A at the chevron, blind A after 5 s, none once the hint shows",
        .input = {.config = arrival_config(), .looks = {
            look_at(0ms),
            look_at(/*at=*/1000ms, NOTHING, {ADVANCE_ARROW_READING}),
            look_at(1500ms, STILL),
            look_at(/*at=*/3000ms, STILL, {ADVANCE_ARROW_READING}),
            look_at(7900ms, STILL),
            look_at(8000ms, STILL),
            look_at(9000ms, STILL),
            look_at(10000ms),
            look_at(/*at=*/14200ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/15000ms, STILL, {CONTROLS_HINT_READING}),
            look_at(/*at=*/15300ms, STILL, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.states = {
            SceneScreen::SCENE_MOVING, SceneScreen::DIALOG, SceneScreen::SCENE_STILL, SceneScreen::DIALOG,
            SceneScreen::SCENE_STILL, SceneScreen::BLIND_PRESS, SceneScreen::SCENE_STILL, SceneScreen::BLIND_PRESS,
            SceneScreen::HINT_SHOWING, SceneScreen::HINT_SHOWING, SceneScreen::CONTROLS_HINT,
        }},
    },
    {
        .name = "the mission banner on two looks ends the scene; a box between them starts the count again",
        .input = {.config = arrival_config(), .looks = {
            look_at(0ms, DIALOG),
            look_at(500ms),
            look_at(/*at=*/6000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/6250ms, DIALOG, {MISSION_BANNER_READING}),
            look_at(/*at=*/6500ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/12000ms, STILL, {MISSION_BANNER_READING}),
        }},
        .expectation = {.states = {
            SceneScreen::DIALOG, SceneScreen::SCENE_MOVING, SceneScreen::BANNER_SHOWING, SceneScreen::DIALOG,
            SceneScreen::BANNER_SHOWING, SceneScreen::MISSION_BANNER,
        }},
    },
    {
        .name = "blind presses are not busy: a minute of them still ends quiet",
        .input = {.config = arrival_config(), .looks = {
            look_at(/*at=*/0ms, NOTHING, {ADVANCE_ARROW_READING}),
            look_at(5000ms, STILL),
            look_at(6000ms, STILL),
            look_at(59000ms, STILL),
            look_at(60000ms, STILL),
        }},
        .expectation = {.states = {
            SceneScreen::DIALOG, SceneScreen::BLIND_PRESS, SceneScreen::SCENE_STILL, SceneScreen::BLIND_PRESS,
            SceneScreen::QUIET_END,
        }},
    },
    {
        .name = "a machine that stops at choices ends on the choice box, after the text before it",
        .input = {.config = choice_config(), .looks = {look_at(0ms, DIALOG), look_at(500ms), look_at(1000ms, CHOICE)}},
        .expectation = {.states = {SceneScreen::DIALOG, SceneScreen::SCENE_MOVING, SceneScreen::CHOICE}},
    },
    {
        .name = "a machine that does not stop at choices takes the choice as text",
        .input = {.config = arrival_config(), .looks = {look_at(0ms, CHOICE)}},
        .expectation = {.states = {SceneScreen::DIALOG}},
    },
    {
        .name = "the mission complete card is busy: A, and no quiet end while it shows",
        .input = {.config = arrival_config(), .looks = {
            look_at(0ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(59000ms, STILL, {MISSION_COMPLETE_READING}),
            look_at(61000ms),
        }},
        .expectation = {.states = {
            SceneScreen::MISSION_COMPLETE, SceneScreen::MISSION_COMPLETE, SceneScreen::SCENE_MOVING,
        }},
    },
    {
        .name = "a machine that stops at a battle ends on the battle HUD, with A every 2 s where no text shows",
        .input = {.config = battle_config(), .looks = {
            look_at(0ms, DIALOG), look_at(500ms), look_at(1000ms, STILL), look_at(2500ms, STILL), look_at(3000ms, BATTLE),
        }},
        .expectation = {.states = {
            SceneScreen::DIALOG, SceneScreen::BLIND_PRESS, SceneScreen::SCENE_STILL, SceneScreen::BLIND_PRESS,
            SceneScreen::BATTLE,
        }},
    },
    {
        .name = "a machine that does not stop at a battle takes the battle HUD as any screen",
        .input = {.config = quiet_config(0ms), .looks = {look_at(0ms, BATTLE)}},
        .expectation = {.states = {SceneScreen::SCENE_MOVING}},
    },
    {
        .name = "without a quiet time a still screen never ends the scene",
        .input = {.config = {}, .looks = {
            look_at(0ms, STILL), look_at(300000ms, STILL), look_at(300100ms, BLACK),
        }},
        .expectation = {.states = {SceneScreen::SCENE_STILL, SceneScreen::SCENE_STILL, SceneScreen::FADE_BLACK}},
    },
};

//  Feeds the looks to a timeline as the machine does: classify, then record
//  the look it acted on.
UnitTestResult check_timeline(const TimelineCase& test){
    if (test.input.looks.size() != test.expectation.states.size()){
        return std::format("{} looks, {} states expected", test.input.looks.size(), test.expectation.states.size());
    }
    SceneTimeline timeline(test.input.config, test.input.looks.front().when);
    for (size_t index = 0; index < test.input.looks.size(); index++){
        const Observation<ZonePose>& look = test.input.looks[index];
        const SceneScreen got = timeline.classify(look);
        const SceneScreen expected = test.expectation.states[index];
        if (got != expected){
            return std::format("look {} ({}): expected {}, got {}",
                index, describe(look.events), scene_screen_name(expected), scene_screen_name(got));
        }
        timeline.record(look);
    }
    return true;
}


struct StateCase{
    std::string name;
    struct Input{
        SceneScreen screen;
    };
    Input input;
    struct Expectation{
        bool terminal = false;
        bool presses = false;
    };
    Expectation expectation;
};

const StateCase STATE_CASES[] = {
    {.name = "a box gets the text button", .input = {SceneScreen::DIALOG}, .expectation = {.presses = true}},
    {.name = "a blind press is A", .input = {SceneScreen::BLIND_PRESS}, .expectation = {.presses = true}},
    {.name = "the choice box ends the scene", .input = {SceneScreen::CHOICE}, .expectation = {.terminal = true}},
    {.name = "a fade waits", .input = {SceneScreen::FADE_WHITE}, .expectation = {}},
    {.name = "a still scene waits", .input = {SceneScreen::SCENE_STILL}, .expectation = {}},
    {.name = "the hint not held yet waits", .input = {SceneScreen::HINT_SHOWING}, .expectation = {}},
    {.name = "the hint held ends the scene", .input = {SceneScreen::CONTROLS_HINT}, .expectation = {.terminal = true}},
    {.name = "the banner's second look ends the scene", .input = {SceneScreen::MISSION_BANNER}, .expectation = {.terminal = true}},
    {.name = "the quiet end ends the scene", .input = {SceneScreen::QUIET_END}, .expectation = {.terminal = true}},
    {.name = "the mission complete card gets A", .input = {SceneScreen::MISSION_COMPLETE}, .expectation = {.presses = true}},
    {.name = "the battle HUD ends the scene", .input = {SceneScreen::BATTLE}, .expectation = {.terminal = true}},
};

UnitTestResult check_state(const StateCase& test){
    const bool terminal = scene_screen_terminal(test.input.screen);
    const bool presses = scene_screen_presses(test.input.screen);
    if (terminal != test.expectation.terminal || presses != test.expectation.presses){
        return std::format("{}: terminal {}, presses {}", scene_screen_name(test.input.screen), terminal, presses);
    }
    return true;
}


struct RunCase{
    std::string name;
    struct Input{
        //  The backend's looks, the last repeating.
        std::vector<Observation<ZonePose>> looks;
        SceneMachineConfig config = arrival_config();
    };
    Input input;
    struct Expectation{
        //  "done", or "failed: <failure>".
        std::string verdict;
        //  How many times A was pressed.
        size_t a_presses = 0;
        //  How many times B was pressed.
        size_t b_presses = 0;
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const RunCase RUN_CASES[] = {
    {
        .name = "01.01's arrival: A at the text, in the flyover and at the choice, done on the controls hint",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(1000ms),
            look_at(21000ms),
            look_at(22000ms, DIALOG),
            look_at(23000ms),
            look_at(/*at=*/24000ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/24500ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/25100ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.verdict = "done", .a_presses = 3, .line = "[PLZA test] done on controls_hint."},
    },
    {
        .name = "the mission banner ends the scene, which then waits for it to go",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(/*at=*/500ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/750ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/1000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(1250ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] the mission banner has gone after 250 ms."},
    },
    {
        //  Two looks end the scene, 40 more are the 10 s wait.
        .name = "a banner still up after 10 s goes after one A (01.03's Main Mission 02)",
        .input = {.looks = banner_then_gone(42)},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] the mission banner has gone after A press 1."},
    },
    {
        .name = "a banner that stays gets A 3 times, then is left",
        .input = {.looks = {
            look_at(/*at=*/0ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/250ms, NOTHING, {MISSION_BANNER_READING}),
        }},
        .expectation = {.verdict = "done", .a_presses = 3, .line = "[PLZA test] the mission banner still shows after 3 A presses; going on."},
    },
    {
        .name = "01.03's end: A at the text and the mission complete card, done on the next banner",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(500ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(1000ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(1500ms),
            look_at(2000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(2250ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(2500ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 3, .line = "[PLZA test] done on mission_banner."},
    },
    {
        .name = "a mission complete card that A does not get past fails as scene_stuck",
        .input = {.looks = {look_at(0ms, NOTHING, {MISSION_COMPLETE_READING})}},
        .expectation = {.verdict = "failed: scene_stuck", .a_presses = 30},
    },
    {
        .name = "story_battle_start: A through the talk and the tutorial card, done on the battle HUD",
        .input = {.looks = {
            look_at(0ms, CHOICE), look_at(500ms, DIALOG), look_at(1000ms), look_at(1500ms, BATTLE),
        }, .config = battle_config()},
        .expectation = {.verdict = "done", .a_presses = 3, .line = "[PLZA test] done on battle."},
    },
    {
        .name = "a fade is waited out; the look that ends the wait is not acted on",
        .input = {.looks = {
            look_at(0ms, WHITE),
            look_at(15000ms, WHITE),
            look_at(16000ms),
            look_at(16500ms, DIALOG),
            look_at(/*at=*/17000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/17250ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(17500ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] fade_white: white fade"},
    },
    {
        .name = "01.02's Pancham: B through the text, done on the choice box",
        .input = {.looks = {look_at(0ms), look_at(5000ms, DIALOG), look_at(6000ms, DIALOG), look_at(7000ms, CHOICE)}, .config = choice_config()},
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 2, .line = "[PLZA test] done on choice."},
    },
    {
        .name = "a minute with nothing going on fails as scene_quiet",
        .input = {.looks = {look_at(0ms), look_at(60000ms)}},
        .expectation = {.verdict = "failed: scene_quiet", .a_presses = 0},
    },
    {
        .name = "a box that A does not get past fails as scene_stuck",
        .input = {.looks = {look_at(0ms, DIALOG)}},
        .expectation = {.verdict = "failed: scene_stuck", .a_presses = 400},
    },
    {
        .name = "past 360 s it fails as scene_timeout",
        .input = {.looks = {look_at(0ms, DIALOG), look_at(361000ms, DIALOG)}},
        .expectation = {.verdict = "failed: scene_timeout", .a_presses = 1},
    },
    {
        .name = "the trace logs each new state",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(/*at=*/500ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/800ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(1000ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] screen 3: mission_banner at 0.8s"},
    },
};

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    const Pokemon::Outcome outcome = run_scene_machine(backend, "test", test.input.config, 360s);
    const std::string verdict = outcome.succeeded() ? "done" : std::format("failed: {}", outcome.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const size_t a_presses = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "press A");
    if (a_presses != test.expectation.a_presses){
        return std::format("expected {} A presses, got {}: {}", test.expectation.a_presses, a_presses, joined(backend.inputs(), ", "));
    }
    const size_t b_presses = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "press B");
    if (b_presses != test.expectation.b_presses){
        return std::format("expected {} B presses, got {}: {}", test.expectation.b_presses, b_presses, joined(backend.inputs(), ", "));
    }
    if (test.expectation.line.empty()){
        return true;
    }
    for (const std::string& line : backend.lines()){
        if (line == test.expectation.line){
            return true;
        }
    }
    return std::format("expected the line \"{}\" in:\n{}", test.expectation.line, joined(backend.lines(), "\n"));
}


class TimelineTest : public UnitTest{
public:
    explicit TimelineTest(const TimelineCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine timeline - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_timeline(m_test);
    }
private:
    const TimelineCase& m_test;
};

class StateTest : public UnitTest{
public:
    explicit StateTest(const StateCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine state - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_state(m_test);
    }
private:
    const StateCase& m_test;
};

class RunTest : public UnitTest{
public:
    explicit RunTest(const RunCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine run - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_run(m_test, scope);
    }
private:
    const RunCase& m_test;
};

} // namespace


void add_tests_AutoStorySceneMachine(UnitTestDatabase& database){
    for (const TimelineCase& test : TIMELINE_CASES){
        database.add<TimelineTest>(test);
    }
    for (const StateCase& test : STATE_CASES){
        database.add<StateTest>(test);
    }
    for (const RunCase& test : RUN_CASES){
        database.add<RunTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
