/*  PokemonLZA AutoStory - Scene machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scene machine on scripted looks, no frames needed: whole runs on a
 *  FakeStoryBackend (their outcome, presses, the states they went through
 *  and their log).
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

//  A quiet time of 8 s after a scene of at least `min_length`, no blind A.
SceneMachineConfig quiet_config(Milliseconds min_length){
    SceneMachineConfig config;
    config.quiet = 8000ms;
    config.min_length = min_length;
    return config;
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
        //  The states the trace logged, joined by ", "; empty: not checked.
        std::string states = {};
    };
    Expectation expectation;
};

const RunCase RUN_CASES[] = {
    {
        .name = "a still frame past the quiet time waits for the minimum length",
        .input = {
            .looks = {
                look_at(0ms), look_at(5000ms, DIALOG), look_at(10000ms, STILL), look_at(30000ms, STILL),
                look_at(59900ms), look_at(60000ms, STILL),
            },
            .config = quiet_config(60000ms),
        },
        .expectation = {
            .verdict = "failed: scene_quiet",
            .a_presses = 1,
            .line = "[PLZA test] screen 4: quiet_end at 60.0s",
            .states = "scene_moving, dialog, scene_still, quiet_end",
        },
    },
    {
        .name = "a fade starts the quiet time again",
        .input = {
            .looks = {
                look_at(0ms, DIALOG), look_at(7900ms, STILL), look_at(7950ms), look_at(15000ms, WHITE),
                look_at(20000ms), look_at(22900ms, STILL), look_at(23000ms, STILL),
            },
            .config = quiet_config(0ms),
        },
        .expectation = {
            .verdict = "failed: scene_quiet",
            .a_presses = 1,
            .line = "[PLZA test] screen 5: quiet_end at 23.0s",
            .states = "dialog, scene_still, fade_white, scene_still, quiet_end",
        },
    },
    {
        .name = "a black fade longer than the quiet time is waited out",
        .input = {
            .looks = {
                look_at(0ms, BLACK), look_at(7000ms, BLACK), look_at(14000ms, BLACK), look_at(21900ms),
                look_at(/*at=*/22000ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/23100ms, NOTHING, {CONTROLS_HINT_READING}),
            },
            .config = quiet_config(0ms),
        },
        .expectation = {.verdict = "done", .a_presses = 0, .states = "fade_black, hint_showing, controls_hint"},
    },
    {
        .name = "the controls hint held a second ends the scene; a box over it starts the second again",
        .input = {
            .looks = {
                look_at(/*at=*/0ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/600ms, DIALOG, {CONTROLS_HINT_READING}),
                look_at(/*at=*/900ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/1800ms, STILL, {CONTROLS_HINT_READING}),
                look_at(/*at=*/9000ms, STILL, {CONTROLS_HINT_READING}),
            },
            .config = quiet_config(0ms),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 1,
            .line = "[PLZA test] screen 4: controls_hint at 9.0s",
            .states = "hint_showing, dialog, hint_showing, controls_hint",
        },
    },
    {
        .name = "the controls hint is not held across a look without it",
        .input = {
            .looks = {
                look_at(/*at=*/0ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(500ms),
                look_at(/*at=*/1200ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/2200ms, NOTHING, {CONTROLS_HINT_READING}),
            },
            .config = quiet_config(0ms),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 0,
            .line = "[PLZA test] screen 4: controls_hint at 2.2s",
            .states = "hint_showing, scene_moving, hint_showing, controls_hint",
        },
    },
    {
        .name = "01.01's ad: A at the chevron, blind A after 5 s, none once the hint shows",
        .input = {.looks = {
            look_at(0ms),
            look_at(/*at=*/1000ms, NOTHING, {ADVANCE_ARROW_READING}),
            look_at(1500ms, STILL),
            look_at(3000ms, STILL),
            look_at(7900ms, STILL),
            look_at(8000ms, STILL),
            look_at(9000ms, STILL),
            look_at(10000ms),
            look_at(/*at=*/14200ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/15000ms, STILL, {CONTROLS_HINT_READING}),
            look_at(/*at=*/15300ms, STILL, {CONTROLS_HINT_READING}),
        }},
        .expectation = {
            .verdict = "done",
            .a_presses = 2,
            .states = "scene_moving, dialog, scene_still, blind_press, scene_still, hint_showing, controls_hint",
        },
    },
    {
        .name = "the mission banner on two looks ends the scene; a box between them starts the count again",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(500ms),
            look_at(/*at=*/6000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/6250ms, DIALOG, {MISSION_BANNER_READING}),
            look_at(/*at=*/6500ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/12000ms, STILL, {MISSION_BANNER_READING}),
        }},
        .expectation = {
            .verdict = "done",
            .a_presses = 2,
            .line = "[PLZA test] screen 6: mission_banner at 12.0s",
            .states = "dialog, scene_moving, banner_showing, dialog, banner_showing, mission_banner",
        },
    },
    {
        .name = "blind presses are not busy: a minute of them still ends quiet",
        .input = {.looks = {
            look_at(/*at=*/0ms, NOTHING, {ADVANCE_ARROW_READING}),
            look_at(5000ms, STILL),
            look_at(6000ms, STILL),
            look_at(59000ms, STILL),
            look_at(60000ms, STILL),
        }},
        .expectation = {
            .verdict = "failed: scene_quiet",
            .a_presses = 2,
            .states = "dialog, blind_press, scene_still, quiet_end",
        },
    },
    {
        .name = "without a quiet time a still screen never ends the scene",
        .input = {
            .looks = {
                look_at(0ms, STILL), look_at(300000ms, STILL),
                look_at(/*at=*/300100ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/301200ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/302500ms, NOTHING, {CONTROLS_HINT_READING}),
            },
            .config = {},
        },
        .expectation = {.verdict = "done", .a_presses = 0, .states = "scene_still, hint_showing, controls_hint"},
    },
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
        .name = "a banner that stays is left after 10 s",
        .input = {.looks = {
            look_at(/*at=*/0ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/250ms, NOTHING, {MISSION_BANNER_READING}),
        }},
        .expectation = {.verdict = "done", .a_presses = 0, .line = "[PLZA test] the mission banner still shows after 10000 ms; going on."},
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
        .name = "a machine that stops at choices ends on the choice box, after the text before it",
        .input = {.looks = {look_at(0ms, DIALOG), look_at(500ms), look_at(1000ms, CHOICE)}, .config = choice_config()},
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 1, .states = "dialog, scene_moving, choice"},
    },
    {
        .name = "a machine that does not stop at choices takes the choice as text",
        .input = {.looks = {
            look_at(0ms, CHOICE),
            look_at(/*at=*/500ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/1600ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .states = "dialog, hint_showing, controls_hint"},
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

//  "dialog, scene_still": the states in the trace's lines ("[PLZA test]
//  screen 2: scene_still at 1.5s").
std::string traced_states(const std::vector<std::string>& lines){
    std::vector<std::string> states;
    for (const std::string& line : lines){
        const size_t screen = line.find("] screen ");
        const size_t name = line.find(": ", screen);
        const size_t at = line.rfind(" at ");
        if (screen != std::string::npos && name != std::string::npos && at != std::string::npos && at > name){
            states.push_back(line.substr(name + 2, at - name - 2));
        }
    }
    return joined(states, ", ");
}

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
    if (!test.expectation.states.empty() && traced_states(backend.lines()) != test.expectation.states){
        return std::format("expected the states \"{}\", got \"{}\"", test.expectation.states, traced_states(backend.lines()));
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
    for (const RunCase& test : RUN_CASES){
        database.add<RunTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
