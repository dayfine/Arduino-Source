/*  PokemonLZA AutoStory - Story game Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's story game with no console (make_plza_story_game()) on a
 *  FakeStoryBackend: the built-in checkpoints, or parts of them, against
 *  scripted looks, so a run's report, inputs and log are checked with no
 *  console.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryGame_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::ZonePose;


std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}


struct StoryDataCase{
    std::string name;
    struct Input{};
    Input input;
    struct Expectation{
        //  "<id>:<new_game|from_save>[:<autosave|unverified>]" per row, in order.
        std::string rows;
    };
    Expectation expectation;
};

const StoryDataCase STORY_DATA_CASES[] = {
    {
        .name = "the built-in story loads and passes the lint",
        .input = {},
        .expectation = {.rows = "01.00:new_game:autosave 01.01:from_save:unverified 01.02:from_save:unverified 01.03:from_save:unverified"},
    },
};

//  The built-in story: its rows in order, what they start from and save.
UnitTestResult check_story_data(const StoryDataCase& test){
    const StoryData& data = builtin_story_data();
    std::vector<std::string> rows;
    for (const StoryCheckpoint& row : data.checkpoints){
        const std::vector<std::string> lint = plza_story_lint(row);
        if (!lint.empty()){
            return std::format("{} fails the lint: {}", row.id, joined(lint, "; "));
        }
        rows.push_back(std::format(
            "{}:{}{}", row.id, story_row_starts_without_save(row) ? "new_game" : "from_save",
            data.unverified_autosaves.contains(row.id) ? ":unverified" : (data.autosaves.contains(row.id) ? ":autosave" : "")
        ));
    }
    const std::string got = joined(rows, " ");
    if (got != test.expectation.rows){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.rows, got);
    }
    return true;
}


struct LintCase{
    std::string name;
    struct Input{
        StoryStep step;
    };
    Input input;
    struct Expectation{
        //  The lint's errors, joined by "; ".
        std::string errors;
    };
    Expectation expectation;
};

const LintCase LINT_CASES[] = {
    {
        .name = "a walk to a white fade passes",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(4500)}}}, {"end", std::string("fade_white")},
        }}},
        .expectation = {.errors = ""},
    },
    {
        .name = "a walk to an unknown end fails",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(4500)}}}, {"end", std::string("sideways")},
        }}},
        .expectation = {.errors = "01.01 w: \"end\" must be \"fade_white\", \"fade_black\" or \"dialog\""},
    },
    {
        .name = "a talk with no action passes",
        .input = {.step = {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}}},
        .expectation = {.errors = ""},
    },
    {
        .name = "an action PLZA does not have fails",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "m", .why = "", .action = "choose_7", .args = {}}},
        .expectation = {.errors = "01.01 step 1 m: unknown action \"choose_7\" for GAME"},
    },
};

UnitTestResult check_lint(const LintCase& test){
    StoryCheckpoint checkpoint;
    checkpoint.id = "01.01";
    checkpoint.title = "Test";
    checkpoint.steps.push_back(test.input.step);
    const std::string got = joined(plza_story_lint(checkpoint), "; ");
    if (got != test.expectation.errors){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.errors, got);
    }
    return true;
}


//  A look with `events` and the readings named in `readings`, `at` after
//  the scripts' common start.
Observation<ZonePose> look(ScreenEvents events, std::vector<std::string_view> readings = {}, Milliseconds at = Milliseconds(0)){
    Observation<ZonePose> observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

const Observation<ZonePose> PLATFORM = look({}, {PLATFORM_READING});
const Observation<ZonePose> AUTOSAVE = look({}, {AUTOSAVE_READING});
const Observation<ZonePose> CUTSCENE = look({});
const Observation<ZonePose> TEXT_FOR_A = look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING});
const Observation<ZonePose> STILL = look({ScreenEvent::SCREEN_STILL});
const Observation<ZonePose> WHITE_FADE = look({ScreenEvent::FADE_WHITE});
const Observation<ZonePose> BLACK_FADE = look({ScreenEvent::FADE_BLACK});
const Observation<ZonePose> DIALOG = look({ScreenEvent::DIALOG_BOX});
const Observation<ZonePose> BATTLE_HUD = look({ScreenEvent::BATTLE_STARTED});

//  A choice box of `rows` rows, row `selected` highlighted.
Observation<ZonePose> choice(int selected, int rows = 2){
    Observation<ZonePose> observation = look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX});
    observation.readings[std::string(CHOICE_ROWS_READING)] = rows;
    observation.readings[std::string(CHOICE_SELECTED_READING)] = selected;
    return observation;
}

//  01.01's walk off the platform into the white fade: the platform held
//  for the start check and the walk's free control, then the fade.
std::vector<Observation<ZonePose>> walk_into_white_fade(){
    return {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, WHITE_FADE};
}

//  `first`, then `then`.
std::vector<Observation<ZonePose>> concat(std::vector<Observation<ZonePose>> first, const std::vector<Observation<ZonePose>>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}

//  `text` `times` times, joined by ", ".
std::string repeated(std::string_view text, size_t times){
    return joined(std::vector<std::string>(times, std::string(text)), ", ");
}


//  The built-in checkpoint `id`, its `steps` steps from step `first_step`
//  (from 0) on.
StoryCheckpoint builtin_row(std::string_view id, size_t first_step, size_t steps){
    for (const StoryCheckpoint& row : builtin_story_data().checkpoints){
        if (row.id == id){
            StoryCheckpoint part = row;
            part.steps.erase(part.steps.begin(), part.steps.begin() + first_step);
            if (steps < part.steps.size()){
                part.steps.resize(steps);
            }
            return part;
        }
    }
    return {};
}


struct RunCase{
    std::string name;
    struct Input{
        //  The built-in checkpoint, its `steps` steps from step `first_step`
        //  (from 0) on.
        std::string checkpoint;
        size_t first_step = 0;
        size_t steps = std::numeric_limits<size_t>::max();
        std::vector<Observation<ZonePose>> looks;
        //  How the backend's battles end.
        Pokemon::Outcome battle = Pokemon::outcome_ok();
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <event>: <failure>".
        std::string verdict;
        //  The backend's inputs, joined by ", "; none: not checked.
        std::optional<std::string> inputs = std::nullopt;
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const RunCase RUN_CASES[] = {
    {
        .name = "01.00 stops at Confirm Choices off a console",
        .input = {.checkpoint = "01.00", .first_step = 0, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at confirm: console_only", .inputs = ""},
    },
    {
        .name = "01.00's opening ends on the platform after the autosave",
        .input = {.checkpoint = "01.00", .first_step = 1, .looks = {CUTSCENE, PLATFORM, AUTOSAVE, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 300ms, wait 300ms, wait 300ms, wait 300ms",
            .line = "[PLZA] platform wait: on the platform after 6 looks",
        },
    },
    {
        .name = "01.00's opening presses A once for the text waiting for it",
        .input = {.checkpoint = "01.00", .first_step = 1, .looks = {TEXT_FOR_A, TEXT_FOR_A, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .inputs = "press A, wait 300ms, wait 300ms, wait 300ms, wait 300ms"},
    },
    {
        .name = "01.00's opening fails with no autosave",
        .input = {.checkpoint = "01.00", .first_step = 1, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at opening: platform_not_seen"},
    },
    {
        .name = "01.01 walks off the platform into the white fade",
        .input = {.checkpoint = "01.01", .first_step = 0, .steps = 2, .looks = walk_into_white_fade()},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 250ms, hold north 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, hold north 250ms walk",
            .line = "[PLZA] platform_to_plaza: the end event after 250 ms of the last leg.",
        },
    },
    {
        .name = "01.01's walk plays a dropped leg again",
        .input = {.checkpoint = "01.01", .first_step = 0, .steps = 2, .looks = {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, STILL, PLATFORM, WHITE_FADE}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 250ms, hold north 4500ms walk, hold north 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, hold north 250ms walk",
            .line = "[PLZA] walk: the screen stayed still after the walk of leg 1/2 turn 0 walk 4500 ms; once more",
        },
    },
    {
        .name = "01.01's walk ends on a black fade",
        .input = {.checkpoint = "01.01", .first_step = 1, .steps = 1, .looks = {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, BLACK_FADE}},
        .expectation = {.verdict = "done", .line = "[PLZA] platform_to_plaza: a black fade after 250 ms of the last leg: the scene takes over."},
    },
    {
        .name = "01.01's walk fails when the fade never shows",
        .input = {.checkpoint = "01.01", .first_step = 1, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at platform_to_plaza: walk_no_end"},
    },
    {
        //  console B, 2026-11-15: control came ~7 s after 01.01's scenes
        //  had ended, with nothing on screen to show it was not there yet.
        .name = "a walk waits for control that comes 7 s after the scene",
        .input = {.checkpoint = "01.01", .first_step = 1, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(28, CUTSCENE),
            {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, WHITE_FADE}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = repeated("wait 250ms", 29) +
                ", hold north 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, hold north 250ms walk",
        },
    },
    {
        .name = "a walk takes the controls hint for control",
        .input = {.checkpoint = "01.01", .first_step = 1, .steps = 1, .looks = {
            look(/*events=*/{}, {CONTROLS_HINT_READING}), look(/*events=*/{}, {CONTROLS_HINT_READING}),
            CUTSCENE, CUTSCENE, CUTSCENE, WHITE_FADE,
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, hold north 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, hold north 250ms walk",
        },
    },
    {
        .name = "01.01's walk does not start in a text box",
        .input = {.checkpoint = "01.01", .first_step = 1, .looks = {DIALOG}},
        .expectation = {.verdict = "failed at platform_to_plaza: walk_not_free"},
    },
    {
        .name = "01.01's start check fails off the platform",
        .input = {.checkpoint = "01.01", .first_step = 0, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at on_platform: platform_not_seen"},
    },
    {
        .name = "01.01 runs from the platform through the arrival to the controls hint",
        .input = {.checkpoint = "01.01", .first_step = 0, .looks = concat(walk_into_white_fade(), {
            WHITE_FADE,
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(1500)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(2000)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(2250)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2500)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(9500)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(10500)),
        })},
        .expectation = {.verdict = "done", .line = "[PLZA mash_to_move_hints] done on controls_hint."},
    },
    {
        //  console B, 2026-11-15: after the ad, the loading screen fades
        //  from black into the rival's talk and Pancham; the mission banner
        //  (which the detector can miss) is followed by a second black
        //  fade, then control.
        .name = "01.01's arrival ends 5 s after its second black fade, past the banner",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(1000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(3000)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(3500)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(3750)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(4000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(5000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(5250)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(10500)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, wait 250ms, press A, wait 250ms, wait 250ms, wait 250ms, wait 500ms",
            .line = "[PLZA mash_to_move_hints] done on fade_settled.",
        },
    },
    {
        .name = "01.01's arrival with the banner and the hint missed ends 5 s after the text that follows its second fade",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(1500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2250)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(3000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(3500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(8500)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, wait 250ms, wait 250ms, wait 500ms, press A, wait 500ms",
            .line = "[PLZA mash_to_move_hints] the mission banner did not show (the detector may have missed it).",
        },
    },
    {
        .name = "01.01's arrival ends on the controls hint before the 5 s after its second fade",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(1500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(2250)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(3250)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, wait 250ms, wait 250ms, wait 250ms",
            .line = "[PLZA mash_to_move_hints] done on controls_hint.",
        },
    },
    {
        .name = "01.01's arrival ends on the controls hint held a second",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(1000)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(1500)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(2000)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, wait 250ms, wait 250ms",
            .line = "[PLZA mash_to_move_hints] done on controls_hint.",
        },
    },
    {
        .name = "01.01's arrival fails after a minute with nothing going on",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(60000)),
        }},
        .expectation = {.verdict = "failed at arrival_scenes: scene_quiet"},
    },
    {
        .name = "01.00 ends on its autosave",
        .input = {.checkpoint = "01.00", .first_step = 1, .looks = {AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .line = "[Save] 01.00: the game autosaved at its end"},
    },
    {
        .name = "01.01 is not saved: its autosave is unverified",
        .input = {.checkpoint = "01.01", .first_step = 0, .steps = 1, .looks = {PLATFORM}},
        .expectation = {.verdict = "done", .line = "[Save] 01.01: not saved (save unverified)"},
    },
    {
        .name = "01.02's Pancham scene: B through the text, done on the choice box",
        .input = {.checkpoint = "01.02", .first_step = 1, .steps = 1, .looks = {DIALOG, choice(0)}},
        .expectation = {.verdict = "done", .inputs = "press B", .line = "[PLZA text_to_choice] done on choice."},
    },
    {
        .name = "01.02 takes the second choice: down from the top row, then A",
        .input = {.checkpoint = "01.02", .first_step = 2, .steps = 1, .looks = {choice(0), choice(1)}},
        .expectation = {.verdict = "done", .line = "[PLZA] choose_1: row 0 of 2 highlighted"},
    },
    {
        .name = "01.02's choice already on the second row: A only",
        .input = {.checkpoint = "01.02", .first_step = 2, .steps = 1, .looks = {choice(1)}},
        .expectation = {.verdict = "done", .inputs = "press A"},
    },
    {
        .name = "01.02's choice fails when the highlight does not move",
        .input = {.checkpoint = "01.02", .first_step = 2, .steps = 1, .looks = {choice(0)}},
        .expectation = {.verdict = "failed at battle_with_pokemon: choice_not_reached"},
    },
    {
        .name = "01.02's choice fails with no choice box",
        .input = {.checkpoint = "01.02", .first_step = 2, .steps = 1, .looks = {DIALOG}},
        .expectation = {.verdict = "failed at battle_with_pokemon: no_choice"},
    },
    {
        .name = "01.02's choice fails on a menu of one row",
        .input = {.checkpoint = "01.02", .first_step = 2, .steps = 1, .looks = {choice(0, 1)}},
        .expectation = {.verdict = "failed at battle_with_pokemon: no_such_choice"},
    },
    {
        .name = "01.02's battle area ends on the controls hint, not saved",
        .input = {.checkpoint = "01.02", .first_step = 3, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(1000)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(2000)),
        }},
        .expectation = {.verdict = "done", .line = "[Save] 01.02: not saved (save unverified)"},
    },
    {
        .name = "01.03 walks to Chikorita: a turn left, then 750 ms",
        .input = {.checkpoint = "01.03", .first_step = 0, .steps = 1, .looks = {look(/*events=*/{}, {CONTROLS_HINT_READING})}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, press L, stick -70deg 150ms walk, release 100ms, press L, hold north 750ms walk",
        },
    },
    {
        .name = "01.03's talk to the starter: A, then the text box",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {CUTSCENE, DIALOG}},
        .expectation = {.verdict = "done", .inputs = "press A, wait 250ms"},
    },
    {
        .name = "01.03's talk fails when no text box shows",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at talk_to_starter: talk_no_text"},
    },
    {
        .name = "01.03's battle start: A through the talk and the tutorial card, done on the battle HUD",
        .input = {.checkpoint = "01.03", .first_step = 2, .steps = 1, .looks = {
            look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(500)),
            look({ScreenEvent::BATTLE_STARTED}, /*readings=*/{}, Milliseconds(1000)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, press A",
            .line = "[PLZA story_battle_start] done on battle.",
        },
    },
    {
        .name = "01.03's battle is the backend's",
        .input = {.checkpoint = "01.03", .first_step = 3, .steps = 1, .looks = {BATTLE_HUD}},
        .expectation = {.verdict = "done", .inputs = "battle"},
    },
    {
        .name = "01.03's battle fails as the backend's battle does",
        .input = {.checkpoint = "01.03", .first_step = 3, .steps = 1, .looks = {BATTLE_HUD},
            .battle = Pokemon::outcome_failed("battle_failed", "no dialogue after the battle")},
        .expectation = {.verdict = "failed at andi_battle: battle_failed"},
    },
    {
        .name = "01.03 ends past the mission complete card on Main Mission 02's banner, not saved",
        .input = {.checkpoint = "01.03", .first_step = 4, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, {MISSION_COMPLETE_READING}, Milliseconds(500)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(1000)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(1250)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1500)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, press A, wait 250ms",
            .line = "[Save] 01.03: not saved (save unverified)",
        },
    },
};

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console(builtin_story_data());
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks, .battle = test.input.battle}, scope);
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    const std::vector<StoryCheckpoint> rows{builtin_row(test.input.checkpoint, test.input.first_step, test.input.steps)};
    const Pokemon::SegmentReport report = engine.run_segment("test", rows, 1, state);
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {}: {}", report.failed_event, report.failure.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const std::string inputs = joined(backend.inputs(), ", ");
    if (test.expectation.inputs && inputs != *test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", *test.expectation.inputs, inputs);
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


//  The console-less game with a reload that presses HOME, as the console's
//  does first, and succeeds.
Pokemon::StoryGame<ZonePose> game_with_reload(const Pokemon::StoryRows& rows){
    Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console(rows);
    game.reload = [](Pokemon::StoryBackend<ZonePose>& backend){
        backend.press(BUTTON_HOME, Milliseconds(80), Milliseconds(80));
        return Pokemon::outcome_ok();
    };
    return game;
}

//  A checkpoint `id` of one GAME step running `action`.
StoryCheckpoint one_step_row(std::string id, std::string action){
    StoryCheckpoint row;
    row.id = std::move(id);
    row.title = "Test";
    row.steps.push_back(StoryStep{.kind = Pokemon::StoryStepKind::GAME, .name = action, .why = "", .action = action, .args = {}});
    return row;
}


struct LoadCase{
    std::string name;
    struct Input{
        std::string checkpoint;     //  A built-in one.
        bool has_reload = true;
        std::vector<Observation<ZonePose>> looks;
    };
    Input input;
    struct Expectation{
        //  "done", or the failure.
        std::string verdict;
        std::string inputs;
        //  A log line the load wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const LoadCase LOAD_CASES[] = {
    {
        .name = "01.02's start: loaded, and off the platform",
        .input = {.checkpoint = "01.02", .looks = {CUTSCENE}},
        .expectation = {
            .verdict = "done", .inputs = "press HOME, wait 300ms, wait 300ms, wait 300ms, wait 300ms",
            .line = "[Checkpoint] 01.02: off the station platform, as at its start",
        },
    },
    {
        .name = "01.02's start on the platform is an older save",
        .input = {.checkpoint = "01.02", .looks = {PLATFORM}},
        .expectation = {.verdict = "older_save", .inputs = "press HOME, wait 300ms"},
    },
    {
        .name = "one platform look is not the platform",
        .input = {.checkpoint = "01.02", .looks = {PLATFORM, CUTSCENE, PLATFORM, CUTSCENE}},
        .expectation = {.verdict = "done", .inputs = "press HOME, wait 300ms, wait 300ms, wait 300ms, wait 300ms"},
    },
    {
        .name = "01.01's start is left to its first step's platform check",
        .input = {.checkpoint = "01.01", .looks = {CUTSCENE}},
        .expectation = {.verdict = "done", .inputs = "press HOME"},
    },
    {
        .name = "a game with no reload cannot load",
        .input = {.checkpoint = "01.02", .has_reload = false, .looks = {CUTSCENE}},
        .expectation = {.verdict = "no_reload", .inputs = ""},
    },
};

UnitTestResult check_load(const LoadCase& test, CancellableScope& scope){
    const StoryData& story = builtin_story_data();
    const Pokemon::StoryGame<ZonePose> game = test.input.has_reload
        ? game_with_reload(story)
        : make_plza_story_game_without_console(story);
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    const Pokemon::Outcome loaded = load_checkpoint_start(game, backend, builtin_row(test.input.checkpoint, /*first_step=*/0, /*steps=*/99));
    const std::string verdict = loaded.succeeded() ? "done" : loaded.failure;
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const std::string inputs = joined(backend.inputs(), ", ");
    if (inputs != test.expectation.inputs){
        return std::format("expected inputs \"{}\", got \"{}\"", test.expectation.inputs, inputs);
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


//  Two rows: "01.01" waits for the platform, "01.02" runs to a choice.
struct ChainCase{
    std::string name;
    struct Input{
        std::set<std::string, std::less<>> reload_when_chained;
        std::vector<Observation<ZonePose>> looks;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  How many times the reload pressed HOME.
        size_t reloads = 0;
    };
    Expectation expectation;
};

const ChainCase CHAIN_CASES[] = {
    {
        .name = "straight through without reload_when_chained",
        .input = {.reload_when_chained = {}, .looks = {PLATFORM, PLATFORM, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "reload_when_chained loads the second row's start first",
        .input = {.reload_when_chained = {"01.02"}, .looks = {PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 1},
    },
    {
        .name = "reload_when_chained stops on an older save",
        .input = {.reload_when_chained = {"01.02"}, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at 01.02 reload_when_chained: older_save", .reloads = 1},
    },
};

UnitTestResult check_chain(const ChainCase& test, CancellableScope& scope){
    Pokemon::StoryRows story;
    story.reload_when_chained = test.input.reload_when_chained;
    const Pokemon::StoryGame<ZonePose> game = game_with_reload(story);
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    Pokemon::StoryEngine<ZonePose> engine(game, backend);
    Pokemon::RunState state;
    const std::vector<StoryCheckpoint> rows{one_step_row("01.01", "wait_for_platform"), one_step_row("01.02", "text_to_choice")};
    const Pokemon::SegmentReport report = run_plza_segments(game, story, rows, /*attempts_per_checkpoint=*/1, engine, backend, state);
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {} {}: {}", report.failed_checkpoint, report.failed_event, report.failure.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const size_t reloads = (size_t)std::count(backend.inputs().begin(), backend.inputs().end(), "press HOME");
    if (reloads != test.expectation.reloads){
        return std::format("expected {} reloads, got {}: {}", test.expectation.reloads, reloads, joined(backend.inputs(), ", "));
    }
    return true;
}


class StoryDataTest : public UnitTest{
public:
    explicit StoryDataTest(StoryDataCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_story_data(m_case);
    }
private:
    StoryDataCase m_case;
};

class LintTest : public UnitTest{
public:
    explicit LintTest(LintCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame lint - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_lint(m_case);
    }
private:
    LintCase m_case;
};

class RunTest : public UnitTest{
public:
    explicit RunTest(RunCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame run - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_run(m_case, scope);
    }
private:
    RunCase m_case;
};

class LoadTest : public UnitTest{
public:
    explicit LoadTest(LoadCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame load - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_load(m_case, scope);
    }
private:
    LoadCase m_case;
};

class ChainTest : public UnitTest{
public:
    explicit ChainTest(ChainCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame chain - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_chain(m_case, scope);
    }
private:
    ChainCase m_case;
};

} // namespace


void add_tests_AutoStoryGame(UnitTestDatabase& database){
    for (const StoryDataCase& test : STORY_DATA_CASES){
        database.add<StoryDataTest>(test);
    }
    for (const LintCase& test : LINT_CASES){
        database.add<LintTest>(test);
    }
    for (const RunCase& test : RUN_CASES){
        database.add<RunTest>(test);
    }
    for (const LoadCase& test : LOAD_CASES){
        database.add<LoadTest>(test);
    }
    for (const ChainCase& test : CHAIN_CASES){
        database.add<ChainTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
