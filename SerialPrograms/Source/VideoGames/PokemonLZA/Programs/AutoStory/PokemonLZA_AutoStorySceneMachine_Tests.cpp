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
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};
const ScreenEvents PARTY{ScreenEvent::PLAYER_CAN_MOVE};

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
    std::vector<Observation<ZonePose>> looks(count, look_at(/*at=*/0ms, NOTHING, {MISSION_BANNER_READING}));
    looks.push_back(look_at(0ms));
    return looks;
}

//  A "scene" step's config: no quiet time, no blind A, the scene must
//  start within 45 s.
SceneMachineConfig scene_step_config(){
    SceneMachineConfig config;
    config.start_window = 45000ms;
    return config;
}

//  Hotel Z's rooms: 3 s quiet is control.
SceneMachineConfig quiet_room_config(){
    SceneMachineConfig config;
    config.quiet = 3000ms;
    config.quiet_is_control = true;
    return config;
}

//  Blind A after 5 s and no quiet time.
SceneMachineConfig blind_press_config(){
    SceneMachineConfig config;
    config.blind_press_after = 5000ms;
    return config;
}

//  The advance chevron, then a cutscene with nothing on it, looked at every
//  500 ms from 5 s to 65 s, then the controls hint held a second.
std::vector<Observation<ZonePose>> minute_of_blind_presses(){
    std::vector<Observation<ZonePose>> looks{look_at(/*at=*/0ms, NOTHING, {ADVANCE_ARROW_READING})};
    for (Milliseconds at = 5000ms; at <= 65000ms; at += 500ms){
        looks.push_back(look_at(at));
    }
    looks.push_back(look_at(/*at=*/65500ms, NOTHING, {CONTROLS_HINT_READING}));
    looks.push_back(look_at(/*at=*/66600ms, NOTHING, {CONTROLS_HINT_READING}));
    return looks;
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
        //  The states the log's lines went through, joined by ", "; empty:
        //  not checked.
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
            .line = "[PLZA test] screen 4: quiet_end at 60.0s: still",
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
            .line = "[PLZA test] screen 5: quiet_end at 23.0s: still",
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
            .line = "[PLZA test] screen 4: controls_hint at 9.0s: still, controls hint",
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
            .line = "[PLZA test] screen 4: controls_hint at 2.2s: controls hint",
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
            look_at(12250ms),
        }},
        .expectation = {
            .verdict = "done",
            .a_presses = 2,
            .line = "[PLZA test] screen 6: mission_banner at 12.0s: still, mission banner",
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
            look_at(/*at=*/500ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(/*at=*/1000ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(1500ms),
            look_at(/*at=*/2000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/2250ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(2500ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 3, .line = "[PLZA test] done on mission_banner."},
    },
    {
        .name = "a mission complete card that A does not get past fails as scene_stuck",
        .input = {.looks = {look_at(/*at=*/0ms, NOTHING, {MISSION_COMPLETE_READING})}},
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
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] screen 1: fade_white at 0.0s: white fade"},
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
        .name = "the mission complete card is busy: A, and no quiet end while it shows",
        .input = {.looks = {
            look_at(/*at=*/0ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(/*at=*/59000ms, STILL, {MISSION_COMPLETE_READING}),
            look_at(61000ms),
            look_at(/*at=*/62000ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/63100ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {
            .verdict = "done",
            .a_presses = 2,
            .states = "mission_complete, scene_moving, hint_showing, controls_hint",
        },
    },
    {
        .name = "a machine that stops at a battle ends on the battle HUD, with A every 2 s where no text shows",
        .input = {
            .looks = {
                look_at(0ms, DIALOG), look_at(500ms), look_at(1000ms, STILL), look_at(2500ms, STILL),
                look_at(2600ms, STILL), look_at(3000ms, BATTLE),
            },
            .config = battle_config(),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 3,
            .states = "dialog, blind_press, scene_still, blind_press, battle",
        },
    },
    {
        .name = "a machine that does not stop at a battle takes the battle HUD as any screen",
        .input = {.looks = {look_at(0ms, BATTLE), look_at(8000ms, BATTLE)}, .config = quiet_config(0ms)},
        .expectation = {.verdict = "failed: scene_quiet", .a_presses = 0, .states = "scene_moving, quiet_end"},
    },
    {
        .name = "01.02's Pancham: B through the text, done on the choice box",
        .input = {.looks = {look_at(0ms), look_at(5000ms, DIALOG), look_at(6000ms, DIALOG), look_at(7000ms, CHOICE)}, .config = choice_config()},
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 2, .line = "[PLZA test] done on choice."},
    },
    {
        .name = "the party bar held a second ends the scene; a box over it starts the second again",
        .input = {
            .looks = {
                look_at(0ms, PARTY), look_at(600ms, DIALOG), look_at(900ms, PARTY), look_at(1800ms, PARTY),
                look_at(1900ms, PARTY),
            },
            .config = quiet_config(0ms),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 1,
            .line = "[PLZA test] screen 4: can_move at 1.9s: party bar",
            .states = "party_bar, dialog, party_bar, can_move",
        },
    },
    {
        //  It shows between the lines of a talk.
        .name = "a machine that stops at a battle does not take the party bar for control",
        .input = {
            .looks = {look_at(0ms, STILL), look_at(1000ms, PARTY), look_at(2500ms, PARTY), look_at(3000ms, BATTLE)},
            .config = battle_config(),
        },
        .expectation = {.verdict = "done", .a_presses = 2, .states = "blind_press, scene_moving, blind_press, battle"},
    },
    {
        .name = "a machine that stops at a choice does not take the party bar for control",
        .input = {
            .looks = {look_at(0ms, PARTY), look_at(1500ms, PARTY), look_at(2000ms, CHOICE)},
            .config = choice_config(),
        },
        .expectation = {.verdict = "done", .a_presses = 0, .states = "scene_moving, choice"},
    },
    {
        .name = "no text, choice or battle within the start window: the scene never started",
        .input = {
            .looks = {look_at(0ms), look_at(10000ms, BLACK), look_at(44900ms, STILL), look_at(45000ms, STILL)},
            .config = scene_step_config(),
        },
        .expectation = {
            .verdict = "failed: scene_never_started",
            .a_presses = 0,
            .states = "scene_moving, fade_black, never_started",
        },
    },
    {
        .name = "a box within the start window: the scene started, a still screen past it waits",
        .input = {
            .looks = {
                look_at(0ms), look_at(10000ms, DIALOG), look_at(50000ms, STILL), look_at(51000ms, PARTY),
                look_at(52000ms, PARTY), look_at(53100ms, PARTY),
            },
            .config = scene_step_config(),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 1,
            .states = "scene_moving, dialog, scene_still, party_bar, can_move",
        },
    },
    {
        .name = "after Brigitte: A at the text, done on the party bar held a second",
        .input = {.looks = {
            look_at(0ms, DIALOG), look_at(500ms, DIALOG), look_at(1000ms, PARTY), look_at(1500ms, PARTY), look_at(2100ms, PARTY),
        }},
        .expectation = {.verdict = "done", .a_presses = 2, .line = "[PLZA test] done on can_move."},
    },
    {
        .name = "Hotel Z's room: the quiet end is control",
        .input = {.looks = {look_at(0ms, DIALOG), look_at(1000ms), look_at(4100ms, STILL)}, .config = quiet_room_config()},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] done: quiet, taken as control."},
    },
    {
        //  console B, 2026-11-13: 02.02 after the Brigitte battle walked
        //  past the street scene's trigger and A-mashed for two minutes.
        .name = "a scene step past its trigger fails as scene_never_started",
        .input = {.looks = {look_at(0ms, BLACK), look_at(2000ms), look_at(46000ms)}, .config = scene_step_config()},
        .expectation = {.verdict = "failed: scene_never_started", .a_presses = 0},
    },
    {
        .name = "a scene step whose scene played out before it ends on the party bar",
        .input = {.looks = {look_at(0ms, PARTY), look_at(1000ms, PARTY)}, .config = scene_step_config()},
        .expectation = {.verdict = "done", .a_presses = 0, .line = "[PLZA test] done on can_move."},
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
        .name = "a minute of blind A in a still cutscene is summed up every 30 s",
        .input = {.looks = minute_of_blind_presses(), .config = blind_press_config()},
        .expectation = {
            .verdict = "done",
            .a_presses = 32,
            .line = "[PLZA test] scene_moving x46, blind_press x15 over 30.0s, 15 presses",
            .states = "dialog, blind_press, scene_moving, blind_press, hint_showing, controls_hint",
        },
    },
    {
        .name = "the trace logs each new state",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(/*at=*/500ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/800ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(1000ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] screen 3: mission_banner at 0.8s: mission banner"},
    },
};

//  "dialog, scene_still": the states in the log's lines ("[PLZA test]
//  screen 2: scene_still at 1.5s: still"), each once where only the
//  readings changed.
std::string traced_states(const std::vector<std::string>& lines){
    std::vector<std::string> states;
    for (const std::string& line : lines){
        const size_t screen = line.find("] screen ");
        if (screen == std::string::npos){
            continue;
        }
        const size_t name = line.find(": ", screen);
        const size_t at = name == std::string::npos ? name : line.find(" at ", name);
        if (at == std::string::npos){
            continue;
        }
        std::string state = line.substr(name + 2, at - name - 2);
        if (states.empty() || states.back() != state){
            states.push_back(std::move(state));
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
