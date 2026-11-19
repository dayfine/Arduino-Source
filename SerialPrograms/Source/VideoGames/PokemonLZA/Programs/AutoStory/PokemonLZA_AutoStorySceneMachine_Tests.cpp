/*  PokemonLZA AutoStory - Scene machine Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scene machine on scripted looks (FakeStoryBackend).
 *
 */
//  code-health: allow file_lines -- one run-case table for the scene machine, sharing the scripted looks above it

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryMenuReadings.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStorySceneMachine.h"
#include "PokemonLZA_AutoStorySceneMachine_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{



std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}

ScreenObservation look_at(Milliseconds at, ScreenEvents events = {}, std::vector<std::string_view> readings = {}){
    ScreenObservation look;
    look.when = WallClock::min() + at;
    look.events = events;
    for (std::string_view reading : readings){
        look.readings[std::string(reading)] = 1;
    }
    return look;
}

ScreenObservation choice_at(Milliseconds at, int rows, int selected){
    ScreenObservation look = look_at(at, {ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX});
    look.readings[std::string(CHOICE_ROWS_READING)] = rows;
    look.readings[std::string(CHOICE_SELECTED_READING)] = selected;
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
const ScreenEvents PROMPT{ScreenEvent::INTERACT_PROMPT};
const ScreenEvents STILL_PROMPT{ScreenEvent::INTERACT_PROMPT, ScreenEvent::SCREEN_STILL};

//  mash_to_move_hints' config: a minute quiet, no blind A.
SceneMachineConfig arrival_config(){
    SceneMachineConfig config;
    config.quiet = 60000ms;
    return config;
}

SceneMachineConfig choice_config(){
    SceneMachineConfig config;
    config.stop_at_choice = true;
    config.text_button = BUTTON_B;
    return config;
}

//  story_battle_start's config.
SceneMachineConfig battle_config(){
    SceneMachineConfig config;
    config.stop_at_battle = true;
    return config;
}

SceneMachineConfig battle_config_locking_on(){
    SceneMachineConfig config = battle_config();
    config.lock_on = [](Milliseconds){
        return true;
    };
    return config;
}

std::vector<ScreenObservation> banner_then_gone(size_t count){
    std::vector<ScreenObservation> looks(count, look_at(/*at=*/0ms, NOTHING, {MISSION_BANNER_READING}));
    looks.push_back(look_at(0ms));
    return looks;
}

SceneMachineConfig scene_step_config(){
    SceneMachineConfig config;
    config.start_window = 45000ms;
    return config;
}

SceneMachineConfig quiet_room_config(){
    SceneMachineConfig config;
    config.quiet = 3000ms;
    config.quiet_is_control = true;
    return config;
}

SceneMachineConfig lobby_config(){
    SceneMachineConfig config = quiet_room_config();
    config.text_button = BUTTON_A;
    return config;
}

//  03.07's gate scene: "received 10 Poke Balls", which no detector reads.
std::vector<ScreenObservation> gift_box_scene(){
    std::vector<ScreenObservation> looks{look_at(0ms, BLACK)};
    for (Milliseconds at = 1500ms; at <= 11500ms; at += 500ms){
        looks.push_back(look_at(at));
    }
    looks.insert(looks.end(), {look_at(11700ms, DIALOG), look_at(/*at=*/12500ms, NOTHING, {CONTROLS_HINT_READING}), look_at(/*at=*/13600ms, NOTHING, {CONTROLS_HINT_READING})});
    return looks;
}

SceneMachineConfig blind_press_config(){
    SceneMachineConfig config;
    config.blind_press_after = 5000ms;
    return config;
}

SceneMachineConfig quiet_blind_press_config(){
    SceneMachineConfig config = blind_press_config();
    config.quiet = 60000ms;
    return config;
}

std::vector<ScreenObservation> minute_of_blind_presses(){
    std::vector<ScreenObservation> looks{look_at(/*at=*/0ms, NOTHING, {ADVANCE_ARROW_READING})};
    for (Milliseconds at = 5000ms; at <= 65000ms; at += 500ms){
        looks.push_back(look_at(at));
    }
    looks.push_back(look_at(/*at=*/65500ms, NOTHING, {CONTROLS_HINT_READING}));
    looks.push_back(look_at(/*at=*/66600ms, NOTHING, {CONTROLS_HINT_READING}));
    return looks;
}

SceneMachineConfig battle_area_config(){
    SceneMachineConfig config = arrival_config();
    config.min_length = 20000ms;
    return config;
}

//  Ryon's "A Parla" prompt, with no party bar or controls hint.
std::vector<ScreenObservation> battle_area_talk_prompt(){
    return {
        look_at(0ms, DIALOG), look_at(500ms), look_at(6000ms),
        look_at(6500ms, PROMPT), look_at(7000ms, PROMPT), look_at(8000ms, STILL_PROMPT),
        look_at(15000ms, STILL_PROMPT), look_at(20000ms, STILL_PROMPT), look_at(20500ms, STILL_PROMPT),
    };
}

SceneMachineConfig quiet_config(Milliseconds min_length){
    SceneMachineConfig config;
    config.quiet = 8000ms;
    config.min_length = min_length;
    return config;
}

SceneMachineConfig three_fades_config(){
    SceneMachineConfig config = arrival_config();
    config.end_after_black_fades = 3;
    return config;
}

//  Each fade's wait ends on the look after it.
std::vector<ScreenObservation> arrival_with_three_fades(){
    return {
        look_at(0ms, DIALOG), look_at(500ms),
        look_at(1000ms, BLACK), look_at(1500ms),
        look_at(2000ms, DIALOG), look_at(2500ms, DIALOG),
        look_at(3000ms, WHITE), look_at(3500ms),
        look_at(4000ms, DIALOG),
        look_at(4500ms, BLACK), look_at(5000ms),
        look_at(/*at=*/5500ms, NOTHING, {MISSION_BANNER_READING}), look_at(/*at=*/6000ms, NOTHING, {MISSION_BANNER_READING}),
        look_at(6500ms, BLACK), look_at(7000ms),
        look_at(7500ms), look_at(12500ms),
    };
}

std::vector<ScreenObservation> arrival_to_second_fade(){
    return {
        look_at(0ms, DIALOG),
        look_at(1000ms, BLACK), look_at(1500ms),
        look_at(2000ms, DIALOG),
        look_at(4500ms, BLACK), look_at(5000ms),
    };
}

std::vector<ScreenObservation> banner_looks(Milliseconds from, Milliseconds every, size_t count){
    std::vector<ScreenObservation> looks;
    for (size_t look = 0; look < count; look++){
        looks.push_back(look_at(/*at=*/from + every * (int64_t)look, NOTHING, {MISSION_BANNER_READING}));
    }
    return looks;
}

//  The Italian banner stays until A.
std::vector<ScreenObservation> arrival_banner_until_a(){
    std::vector<ScreenObservation> looks = arrival_to_second_fade();
    for (const ScreenObservation& look : banner_looks(/*from=*/5500ms, /*every=*/500ms, /*count=*/5)){
        looks.push_back(look);
    }
    for (const ScreenObservation& look : {look_at(8000ms), look_at(8500ms, BLACK), look_at(9000ms), look_at(9500ms), look_at(14500ms)}){
        looks.push_back(look);
    }
    return looks;
}

std::vector<ScreenObservation> arrival_banner_stays(){
    std::vector<ScreenObservation> looks = arrival_to_second_fade();
    for (const ScreenObservation& look : banner_looks(/*from=*/5500ms, /*every=*/500ms, /*count=*/20)){
        looks.push_back(look);
    }
    return looks;
}


struct RunCase{
    std::string name;
    struct Input{
        //  The last repeats.
        std::vector<ScreenObservation> looks;
        SceneMachineConfig config = arrival_config();
    };
    Input input;
    struct Expectation{
        //  "done", or "failed: <failure>".
        std::string verdict;
        size_t a_presses = 0;
        size_t b_presses = 0;
        //  Plus held to skip a cutscene.
        size_t plus_presses = 0;
        //  Empty: not checked.
        std::string line = {};
        //  Joined by ", "; empty: not checked.
        std::string states = {};
        //  log() lines, not log_detail().
        std::optional<std::vector<std::string>> overlay = std::nullopt;
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
            .a_presses = 0,
            .b_presses = 1,
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
            .a_presses = 0,
            .b_presses = 1,
            .line = "[PLZA test] screen 5: quiet_end at 23.0s: still",
            .states = "dialog, scene_still, fade_white, scene_still, quiet_end",
        },
    },
    {.name = "gap_press: a box no detector reads inside the scene gets one A after 2 s", .input = {.looks = gift_box_scene(), .config = [](){
        SceneMachineConfig config = arrival_config();
        config.gap_press = 2000ms;
        return config;
    }()}, .expectation = {.verdict = "done", .a_presses = 1, .b_presses = 1, .plus_presses = 1}},
    {.name = "no gap_press: the same box gets no press", .input = {.looks = gift_box_scene()}, .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 1, .plus_presses = 1}},
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
            .a_presses = 0,
            .b_presses = 1,
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
        .name = "01.01's ad: A at the chevron, none on the still screen after it or once the hint shows",
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
            .a_presses = 1,
            .states = "scene_moving, dialog, scene_still, hint_showing, controls_hint",
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
            .a_presses = 0,
            .b_presses = 2,
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
        }, .config = quiet_blind_press_config()},
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
        .name = "01.01's arrival: B at the text, Plus in the flyover, done on the controls hint",
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
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 2, .plus_presses = 1, .line = "[PLZA test] done on controls_hint."},
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
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 1, .line = "[PLZA test] the mission banner has gone after 250 ms."},
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
        .name = "01.03's end: B at the text, A at the mission complete card, done on the next banner",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(/*at=*/500ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(/*at=*/1000ms, NOTHING, {MISSION_COMPLETE_READING}),
            look_at(1500ms),
            look_at(/*at=*/2000ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/2250ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(2500ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 2, .b_presses = 1, .line = "[PLZA test] done on mission_banner."},
    },
    {
        .name = "a mission complete card that A does not get past fails as scene_stuck",
        .input = {.looks = {look_at(/*at=*/0ms, NOTHING, {MISSION_COMPLETE_READING})}},
        .expectation = {.verdict = "failed: scene_stuck", .a_presses = 30},
    },
    {
        .name = "story_battle_start: A through the talk and the tutorial card, done on the battle HUD",
        .input = {.looks = {
            look_at(0ms, CHOICE), look_at(500ms, DIALOG), look_at(/*at=*/1000ms, NOTHING, {TUTORIAL_CARD_READING}), look_at(1500ms, BATTLE),
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
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 1, .line = "[PLZA test] screen 1: fade_white at 0.0s: white fade"},
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
        .name = "a choice with its second row highlighted: Up, then A on the top row",
        .input = {.looks = {
            choice_at(/*at=*/0ms, /*rows=*/2, /*selected=*/1),
            choice_at(/*at=*/500ms, /*rows=*/2, /*selected=*/0),
            look_at(/*at=*/1000ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/2100ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] choice: 2 rows, row 1 highlighted: Up to row 0"},
    },
    {
        //  B on a choice box jumps to its last (cancel) row.
        .name = "a choice left on its last row by a B: Up to the top row, then A",
        .input = {.looks = {
            choice_at(/*at=*/0ms, /*rows=*/3, /*selected=*/2),
            choice_at(/*at=*/500ms, /*rows=*/3, /*selected=*/1),
            choice_at(/*at=*/1000ms, /*rows=*/3, /*selected=*/0),
            look_at(/*at=*/1500ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/2600ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {.verdict = "done", .a_presses = 1, .line = "[PLZA test] choice: 3 rows, row 0 highlighted: A"},
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
        .name = "a machine that stops at a battle ends on the battle HUD, with no A where no text shows",
        .input = {
            .looks = {
                look_at(0ms, DIALOG), look_at(500ms), look_at(1000ms, STILL), look_at(2500ms, STILL),
                look_at(2600ms, STILL), look_at(3000ms, BATTLE),
            },
            .config = battle_config(),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 1,
            .states = "dialog, scene_moving, scene_still, battle",
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
            .a_presses = 0,
            .b_presses = 1,
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
        .expectation = {.verdict = "done", .a_presses = 0, .states = "scene_still, scene_moving, battle"},
    },
    {
        .name = "after a battle start's fade, the party bar held a second with no battle HUD: ZL held, the move symbols lock on",
        .input = {
            .looks = {look_at(0ms, BLACK), look_at(1000ms, PARTY), look_at(1000ms, PARTY), look_at(2100ms, PARTY), look_at(2200ms, PARTY)},
            .config = battle_config_locking_on(),
        },
        .expectation = {
            .verdict = "done",
            .a_presses = 0,
            .line = "[PLZA test] lock-on 1/5: ZL held: the move symbols showed.",
            .states = "fade_black, scene_moving, lock_on, locked_on",
        },
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
        .name = "the party bar up through the start window and no scene: the scene never started",
        .input = {
            .looks = {look_at(0ms, PARTY), look_at(10000ms, PARTY), look_at(44900ms, PARTY), look_at(45000ms, PARTY)},
            .config = scene_step_config(),
        },
        .expectation = {
            .verdict = "failed: scene_never_started",
            .a_presses = 0,
            .states = "awaiting_scene, never_started",
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
            .a_presses = 0,
            .b_presses = 1,
            .states = "scene_moving, dialog, scene_still, party_bar, can_move",
        },
    },
    {
        .name = "after Brigitte: B at the text, done on the party bar held a second",
        .input = {.looks = {
            look_at(0ms, DIALOG), look_at(500ms, DIALOG), look_at(1000ms, PARTY), look_at(1500ms, PARTY), look_at(2100ms, PARTY),
        }},
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 2, .line = "[PLZA test] done on can_move."},
    },
    {
        .name = "01.02's battle area: the talk prompt held past the minimum length is control, with no A at it",
        .input = {.looks = battle_area_talk_prompt(), .config = battle_area_config()},
        .expectation = {
            .verdict = "done",
            .a_presses = 0,
            .b_presses = 1,
            .plus_presses = 1,
            .line = "[PLZA test] done on talk_prompt.",
            .states = "dialog, scene_moving, skip_cutscene, prompt_showing, talk_prompt",
        },
    },
    {
        .name = "a machine that stops at a battle does not take the talk prompt for control",
        .input = {.looks = {look_at(0ms, PROMPT), look_at(2500ms, PROMPT), look_at(3000ms, BATTLE)}, .config = battle_config()},
        .expectation = {.verdict = "done", .a_presses = 0, .states = "scene_moving, battle"},
    },
    {
        .name = "Hotel Z's room: the quiet end is control",
        .input = {.looks = {look_at(0ms, DIALOG), look_at(1000ms), look_at(4100ms, STILL)}, .config = quiet_room_config()},
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 1, .line = "[PLZA test] done: quiet, taken as control."},
    },
    {
        .name = "Hotel Z's lobby with advance_with_a: A, not B, on its text",
        .input = {.looks = {look_at(0ms, DIALOG), look_at(1000ms), look_at(4100ms, STILL)}, .config = lobby_config()},
        .expectation = {.verdict = "done", .a_presses = 1, .b_presses = 0, .line = "[PLZA test] done: quiet, taken as control."},
    },
    {
        .name = "a scene step's textless cutscene past the start window: the scene is under way, done on the party bar",
        .input = {
            .looks = {
                look_at(0ms, BLACK), look_at(2000ms), look_at(46000ms), look_at(47000ms, PARTY), look_at(48100ms, PARTY),
            },
            .config = scene_step_config(),
        },
        .expectation = {.verdict = "done", .a_presses = 0, .line = "[PLZA test] done on can_move."},
    },
    {
        .name = "a scene step past its trigger, the controls hint up for the start window, fails as scene_never_started",
        .input = {
            .looks = {
                look_at(/*at=*/0ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/1500ms, NOTHING, {CONTROLS_HINT_READING}),
                look_at(/*at=*/45000ms, NOTHING, {CONTROLS_HINT_READING}),
            },
            .config = scene_step_config(),
        },
        .expectation = {.verdict = "failed: scene_never_started", .a_presses = 0, .states = "awaiting_scene, never_started"},
    },
    {
        .name = "a minute with nothing going on fails as scene_quiet",
        .input = {.looks = {look_at(0ms), look_at(60000ms)}},
        .expectation = {.verdict = "failed: scene_quiet", .a_presses = 0},
    },
    {
        .name = "a box that B does not get past fails as scene_stuck",
        .input = {.looks = {look_at(0ms, DIALOG)}},
        .expectation = {.verdict = "failed: scene_stuck", .a_presses = 0, .b_presses = 400},
    },
    {
        .name = "past 360 s it fails as scene_timeout",
        .input = {.looks = {look_at(0ms, DIALOG), look_at(361000ms, DIALOG)}},
        .expectation = {.verdict = "failed: scene_timeout", .a_presses = 0, .b_presses = 1},
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
        .name = "the overlay shows the black fades, the banner and the end; the file log every screen",
        .input = {.looks = arrival_with_three_fades(), .config = three_fades_config()},
        .expectation = {
            .verdict = "done",
            .a_presses = 0,
            .b_presses = 4,
            .states = "dialog, scene_moving, fade_black, dialog, fade_white, dialog, fade_black, banner_showing, "
                "fade_black, scene_moving, fade_settled",
            .overlay = std::vector<std::string>{
                "[PLZA test] start, expecting black fade 3 settled, the controls hint, the party bar or the A prompt.",
                "[PLZA test] screen 3: fade_black at 1.0s: black fade, fade 1/3",
                "[PLZA test] screen 7: fade_black at 4.5s: black fade, fade 2/3",
                "[PLZA test] screen 8: banner_showing at 5.5s: mission banner",
                "[PLZA test] screen 9: fade_black at 6.5s: black fade, fade 3/3",
                "[PLZA test] the mission banner showed.",
                "[PLZA test] done on fade_settled.",
            },
        },
    },
    {
        .name = "with end_after_black_fades, a banner up 2 s gets A, and the run goes on to the third fade",
        .input = {.looks = arrival_banner_until_a(), .config = three_fades_config()},
        .expectation = {
            .verdict = "done",
            .a_presses = 1,
            .b_presses = 2,
            .line = "[PLZA test] done on fade_settled.",
            .states = "dialog, fade_black, dialog, fade_black, banner_showing, banner_press, scene_moving, fade_black, "
                "scene_moving, fade_settled",
        },
    },
    {
        .name = "with end_after_black_fades, a banner that 3 A presses 1 s apart do not clear fails as scene_stuck",
        .input = {.looks = arrival_banner_stays(), .config = three_fades_config()},
        .expectation = {.verdict = "failed: scene_stuck", .a_presses = 3, .b_presses = 2},
    },
    {
        .name = "the trace logs each new state",
        .input = {.looks = {
            look_at(0ms, DIALOG),
            look_at(/*at=*/500ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(/*at=*/800ms, NOTHING, {MISSION_BANNER_READING}),
            look_at(1000ms),
        }},
        .expectation = {.verdict = "done", .a_presses = 0, .b_presses = 1, .line = "[PLZA test] screen 3: mission_banner at 0.8s: mission banner"},
    },
    {
        .name = "a cutscene moving 3 s with no text or HUD gets Plus, then A on the skip prompt's default row",
        .input = {.looks = {
            look_at(0ms, BLACK), look_at(1000ms), look_at(2000ms), look_at(5000ms), choice_at(/*at=*/7000ms, /*rows=*/2, /*selected=*/1),
            look_at(8000ms, BLACK), look_at(9000ms), look_at(/*at=*/10000ms, NOTHING, {CONTROLS_HINT_READING}),
            look_at(/*at=*/11100ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {
            .verdict = "done", .a_presses = 1, .b_presses = 0, .plus_presses = 1,
            .states = "fade_black, scene_moving, skip_cutscene, skip_prompt, fade_black, hint_showing, controls_hint",
        },
    },
    {
        .name = "a cutscene Plus does not skip gets one Plus until the next text or HUD",
        .input = {.looks = {
            look_at(0ms, DIALOG), look_at(1000ms), look_at(4000ms), look_at(7000ms), look_at(10000ms), look_at(12000ms),
            look_at(/*at=*/13000ms, NOTHING, {CONTROLS_HINT_READING}), look_at(/*at=*/14100ms, NOTHING, {CONTROLS_HINT_READING}),
        }},
        .expectation = {
            .verdict = "done", .a_presses = 0, .b_presses = 1, .plus_presses = 1,
            .states = "dialog, scene_moving, skip_cutscene, scene_moving, hint_showing, controls_hint",
        },
    },
    {
        .name = "the map Plus opens on free control gets B",
        .input = {.looks = {
            look_at(0ms, DIALOG), look_at(1000ms), look_at(4000ms), look_at(/*at=*/6500ms, STILL, {MAP_READING}),
            look_at(7000ms, PARTY), look_at(8100ms, PARTY),
        }},
        .expectation = {
            .verdict = "done", .a_presses = 0, .b_presses = 2, .plus_presses = 1,
            .states = "dialog, scene_moving, skip_cutscene, map, party_bar, can_move",
        },
    },
};

//  "dialog, scene_still", each once where only the readings changed.
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
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    const std::optional<Pokemon::StoryFailure> outcome = run_scene_machine(backend, "test", test.input.config, 360s);
    const std::string verdict = !outcome ? "done" : std::format("failed: {}", outcome->code);
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
    const size_t plus_presses = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "press +");
    if (plus_presses != test.expectation.plus_presses){
        return std::format("expected {} Plus presses, got {}: {}", test.expectation.plus_presses, plus_presses, joined(backend.inputs(), ", "));
    }
    if (!test.expectation.states.empty() && traced_states(backend.lines()) != test.expectation.states){
        return std::format("expected the states \"{}\", got \"{}\"", test.expectation.states, traced_states(backend.lines()));
    }
    if (test.expectation.overlay && backend.overlay_lines() != *test.expectation.overlay){
        return std::format(
            "expected the overlay lines:\n{}\ngot:\n{}", joined(*test.expectation.overlay, "\n"), joined(backend.overlay_lines(), "\n")
        );
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


//  03.05's nurse as a closed loop: A at her closing box or prompt talks to
//  her again; B does nothing.
class NurseCounter : public Pokemon::FakeStoryBackend{
public:
    explicit NurseCounter(CancellableScope& scope)
        : FakeStoryBackend({}, scope)
    {}

    //  The talks started after the heal's.
    size_t talks_again() const{
        return m_talks_again;
    }

    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        FakeStoryBackend::press(button, hold, release);
        m_now += hold + release;
        switch (m_phase){
        case Phase::GREETING:
        case Phase::LAST_LINES:
        case Phase::AGAIN:
            m_lines--;
            if (m_lines == 0 && m_phase == Phase::GREETING){
                m_phase = Phase::HEAL_FADE;
                m_fade_until = m_now + 3000ms;
            }else if (m_lines == 0){
                m_phase = Phase::CLOSING;
            }
            return;
        case Phase::CLOSING:
        case Phase::AT_PROMPT:
            if (button == BUTTON_A){
                m_phase = Phase::AGAIN;
                m_lines = 1;
                m_talks_again++;
            }
            return;
        case Phase::HEAL_FADE:
            return;
        }
    }
    virtual void wait(Milliseconds duration) override{
        FakeStoryBackend::wait(duration);
        m_now += duration;
    }
    virtual ScreenObservation observe() override{
        ScreenObservation look;
        look.when = WallClock::min() + m_now;
        if (m_phase == Phase::HEAL_FADE && m_now >= m_fade_until){
            m_phase = Phase::LAST_LINES;
            m_lines = 2;
        }
        switch (m_phase){
        case Phase::HEAL_FADE:
            look.events.set(ScreenEvent::FADE_BLACK, true);
            return look;
        case Phase::GREETING:
        case Phase::LAST_LINES:
        case Phase::AGAIN:
            look.events.set(ScreenEvent::DIALOG_BOX, true);
            look.readings[std::string(ADVANCE_ARROW_READING)] = 1;
            return look;
        case Phase::CLOSING:
            look.events.set(ScreenEvent::DIALOG_BOX, true);
            m_phase = Phase::AT_PROMPT;
            return look;
        case Phase::AT_PROMPT:
            look.events.set(ScreenEvent::INTERACT_PROMPT, true);
            return look;
        }
        return look;
    }

private:
    enum class Phase{GREETING, HEAL_FADE, LAST_LINES, CLOSING, AT_PROMPT, AGAIN};

    Milliseconds m_now = 0ms;
    Phase m_phase = Phase::GREETING;
    size_t m_lines = 5;
    Milliseconds m_fade_until = 0ms;
    size_t m_talks_again = 0;
};

struct NurseCase{
    std::string name;
    struct Input{
        SceneMachineConfig config;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed: <failure>".
        std::string verdict;
        std::string end;
        size_t talks_again = 0;
    };
    Expectation expectation;
};

const NurseCase NURSE_CASES[] = {
    {
        .name = "03.05's heal_party (scene): done on her A prompt, no talk again",
        .input = {.config = scene_step_config()},
        .expectation = {.verdict = "done", .end = "[PLZA test] done on talk_prompt.", .talks_again = 0},
    },
    {
        .name = "mash_to_move_hints at a talk's end: done on the A prompt, no talk again",
        .input = {.config = arrival_config()},
        .expectation = {.verdict = "done", .end = "[PLZA test] done on talk_prompt.", .talks_again = 0},
    },
};

UnitTestResult check_nurse(const NurseCase& test, CancellableScope& scope){
    NurseCounter backend(scope);
    const std::optional<Pokemon::StoryFailure> outcome = run_scene_machine(backend, "test", test.input.config, 120s);
    const std::string verdict = !outcome ? "done" : std::format("failed: {}", outcome->code);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    if (backend.talks_again() != test.expectation.talks_again){
        return std::format(
            "expected {} talks after the heal's, got {}: {}", test.expectation.talks_again, backend.talks_again(), joined(backend.inputs(), ", ")
        );
    }
    if (backend.lines().empty() || backend.lines().back() != test.expectation.end){
        return std::format("expected the run to end with \"{}\"; log:\n{}", test.expectation.end, joined(backend.lines(), "\n"));
    }
    return true;
}

class NurseTest : public UnitTest{
public:
    explicit NurseTest(const NurseCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStorySceneMachine closed loop - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_nurse(m_test, scope);
    }
private:
    const NurseCase& m_test;
};


} // namespace


void add_tests_AutoStorySceneMachine(UnitTestDatabase& database){
    for (const RunCase& test : RUN_CASES){
        database.add<RunTest>(test);
    }
    for (const NurseCase& test : NURSE_CASES){
        database.add<NurseTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
