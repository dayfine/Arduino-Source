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
//  code-health: allow file_lines -- one table per story game behavior, sharing the scripted looks above them

#include <algorithm>
#include <format>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryGame_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::StoryActionCall;
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
        //  "<id>:<new_game|from_save>[:choices][:<autosave|unverified>]" per
        //  row, in order; "choices": the row plays the new game's choices.
        std::string rows;
    };
    Expectation expectation;
};

const StoryDataCase STORY_DATA_CASES[] = {
    {
        .name = "the built-in story loads and passes the lint",
        .input = {},
        .expectation = {.rows =
            "01.00a:new_game:choices 01.00b:new_game:autosave 01.01:from_save:unverified"
        },
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
            "{}:{}{}{}", row.id, story_row_starts_without_save(row) ? "new_game" : "from_save",
            story_row_makes_new_game_choices(row) ? ":choices" : "",
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
        //  The steps before `step` in its checkpoint.
        std::vector<StoryStep> before = {};
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
        .name = "a walk until a scene passes",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(8000)}}}, {"end", std::string("scene")},
        }}},
        .expectation = {.errors = ""},
    },
    {
        .name = "a walk to an unknown end fails",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(4500)}}}, {"end", std::string("sideways")},
        }}},
        .expectation = {.errors = "01.01 w: \"end\" must be \"fade_white\", \"fade_black\", \"dialog\", \"prompt\" or \"scene\""},
    },
    {
        .name = "a walk with a time before its end but no end fails",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(3000)}}}, {"end_after_ms", int64_t(1500)},
        }}},
        .expectation = {.errors = "01.01 w: \"end_after_ms\" needs an \"end\""},
    },
    {
        .name = "a walk to an A prompt passes",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(3000)}}}, {"end", std::string("prompt")},
        }}},
        .expectation = {.errors = ""},
    },
    {
        .name = "a walk with a heading passes",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(4500)}}}, {"heading", int64_t(264)},
        }}},
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
    checkpoint.steps = test.input.before;
    checkpoint.steps.push_back(test.input.step);
    const std::string got = joined(plza_story_lint(checkpoint), "; ");
    if (got != test.expectation.errors){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.errors, got);
    }
    return true;
}

//  A checkpoint "03.06" of `steps`, the plan's "save" `save` and its
//  "autosave_step" `autosave_step`.
struct AutosaveStepCase{
    std::string name;
    struct Input{
        std::vector<std::string> steps;
        Pokemon::StoryRowSave save = Pokemon::StoryRowSave::NONE;
        std::optional<std::string> autosave_step = std::nullopt;
    };
    Input input;
    struct Expectation{
        //  The error, empty for none.
        std::string error;
    };
    Expectation expectation;
};

const AutosaveStepCase AUTOSAVE_STEP_CASES[] = {
    {
        //  03.07 reloaded 03.06's tutorial autosave, outside Wild Zone 1.
        .name = "an autosave before steps that move the player fails",
        .input = {
            .steps = {"wild_zones_tutorial", "to_zone_1_gate", "enter_zone_1"},
            .autosave_step = "wild_zones_tutorial",
        },
        .expectation = {.error =
            "03.06: the game autosaves in wild_zones_tutorial, before to_zone_1_gate: the next checkpoint reloads "
            "that save, so end 03.06 at wild_zones_tutorial and start the next one with to_zone_1_gate"
        },
    },
    {
        .name = "an autosave in the last step passes",
        .input = {
            .steps = {"toward_zone_1_gate", "wild_zones_tutorial"},
            .save = Pokemon::StoryRowSave::AUTOSAVE,
            .autosave_step = "wild_zones_tutorial",
        },
        .expectation = {.error = ""},
    },
    {
        .name = "a plan autosave without its step fails",
        .input = {.steps = {"wild_zones_tutorial"}, .save = Pokemon::StoryRowSave::AUTOSAVE},
        .expectation = {.error = "03.06: \"save\": \"autosave\" needs \"autosave_step\", the step a run log showed the badge in"},
    },
    {
        .name = "an autosave step that is not a step fails",
        .input = {.steps = {"wild_zones_tutorial"}, .autosave_step = "enter_zone_1"},
        .expectation = {.error = "03.06: autosave_step enter_zone_1 is not one of its steps"},
    },
};

UnitTestResult check_autosave_step(const AutosaveStepCase& test){
    StoryCheckpoint checkpoint;
    checkpoint.id = "03.06";
    checkpoint.title = "Test";
    for (const std::string& name : test.input.steps){
        checkpoint.steps.push_back(StoryStep{.kind = Pokemon::StoryStepKind::GAME, .name = name, .why = "", .action = "walk", .args = {}});
    }
    const std::optional<std::string> error = autosave_step_error(checkpoint, test.input.save, test.input.autosave_step);
    const std::string got = error.value_or("");
    if (got != test.expectation.error){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.error, got);
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
//  The autosave badge 2 s after a checkpoint's first look: its own save. A
//  badge in a checkpoint's first 1.5 s is the save before it
//  (AutosaveSightings::note()).
const Observation<ZonePose> OWN_AUTOSAVE = look({}, {AUTOSAVE_READING}, Milliseconds(2000));
const Observation<ZonePose> CUTSCENE = look({});
const Observation<ZonePose> TEXT_FOR_A = look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING});
const Observation<ZonePose> STILL = look({ScreenEvent::SCREEN_STILL});
const Observation<ZonePose> WHITE_FADE = look({ScreenEvent::FADE_WHITE});
const Observation<ZonePose> BLACK_FADE = look({ScreenEvent::FADE_BLACK});
const Observation<ZonePose> DIALOG = look({ScreenEvent::DIALOG_BOX});


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
        //  What the minimap arrow reads, in order (the last repeats); none:
        //  the game has no minimap arrow, as off a console.
        std::vector<double> arrow_reads = {};
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
        .name = "01.00a stops at the new game off a console",
        .input = {.checkpoint = "01.00a", .first_step = 0, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at new_game: console_only", .inputs = ""},
    },
    {
        .name = "01.00b stops at Confirm Choices off a console",
        .input = {.checkpoint = "01.00b", .first_step = 0, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at confirm: console_only", .inputs = ""},
    },
    {
        .name = "01.00b's opening ends on the platform after the autosave",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {CUTSCENE, PLATFORM, AUTOSAVE, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 300ms, wait 300ms, wait 300ms, wait 300ms",
            .line = "[PLZA] platform wait: on the platform after 6 looks",
        },
    },
    {
        .name = "01.00b's opening presses A once for the text waiting for it",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {TEXT_FOR_A, TEXT_FOR_A, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .inputs = "press A, wait 300ms, wait 300ms, wait 300ms, wait 300ms"},
    },
    {
        .name = "01.00b's opening fails with no autosave",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at opening: platform_not_seen"},
    },
    {
        .name = "01.01 walks off the platform into the white fade",
        .input = {.checkpoint = "01.01", .first_step = 0, .steps = 2, .looks = walk_into_white_fade()},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 250ms, stick 0deg 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, stick 0deg 250ms walk",
            .line = "[PLZA] platform_to_plaza: the end event after 250 ms of the last leg.",
        },
    },
    {
        .name = "01.01's walk plays a dropped leg again",
        .input = {.checkpoint = "01.01", .first_step = 0, .steps = 2, .looks = {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, STILL, PLATFORM, WHITE_FADE}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 250ms, stick 0deg 4500ms walk, stick 0deg 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, stick 0deg 250ms walk",
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
        //  Control comes ~7 s after 01.01's scenes end (console B, 2026-11-15).
        .name = "a walk waits for control that comes 7 s after the scene",
        .input = {.checkpoint = "01.01", .first_step = 1, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(28, CUTSCENE),
            {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, WHITE_FADE}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = repeated("wait 250ms", 29) +
                ", stick 0deg 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, stick 0deg 250ms walk",
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
            .inputs = "wait 250ms, stick 0deg 4500ms walk, press L, stick 90deg 150ms walk, release 100ms, press L, stick 0deg 250ms walk",
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
        //  01.02: talk, fade, Pancham, banner, fade, control; 5 s quiet after the
        //  banner is not the end.
        .name = "01.01's arrival ends 5 s after its third black fade, not after the banner that follows its second",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(1000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(2500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(3000)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(3500)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(4000)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(4250)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(4500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(9500)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(10000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(11000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(11250)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(16250)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press B, wait 250ms, wait 250ms, press B, wait 250ms, wait 250ms, wait 500ms, wait 500ms, wait 250ms, wait 500ms",
            .line = "[PLZA mash_to_move_hints] done on fade_settled.",
        },
    },
    {
        .name = "01.01's arrival with the banner and the hint missed ends 5 s after the text that follows its third fade",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(1500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2250)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(2500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2750)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(3000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(3500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(8500)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press B, wait 250ms, wait 250ms, wait 500ms, wait 250ms, press B, wait 500ms",
            .line = "[PLZA mash_to_move_hints] the mission banner did not show (the detector may have missed it).",
        },
    },
    {
        .name = "01.01's arrival ends on the controls hint before the 5 s after its third fade",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(1500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(2100)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2200)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(2250)),
            look(/*events=*/{}, {CONTROLS_HINT_READING}, Milliseconds(3250)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press B, wait 250ms, wait 250ms, wait 250ms, wait 250ms",
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
            .inputs = "press B, wait 250ms, wait 250ms",
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
        //  The badge 2 s in: one in the first 1.5 s would be the save
        //  before 01.00b.
        .name = "01.00b's autosave badge is logged in the step it shows in",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {CUTSCENE, OWN_AUTOSAVE, OWN_AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .line = "[PLZA] autosave badge at 2.0s in opening"},
    },
    {
        //  A first look, then the badge 2 s in: one on the first look would
        //  be the save before 01.00b.
        .name = "01.00b ends on its autosave",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {CUTSCENE, OWN_AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .line = "[Save] 01.00b: the game autosaved (seen)"},
    },
    {
        .name = "01.01 is not saved: its autosave is unverified",
        .input = {.checkpoint = "01.01", .first_step = 0, .steps = 1, .looks = {PLATFORM}},
        .expectation = {.verdict = "done", .line = "[Save] 01.01: not saved (save unverified)"},
    },
    //  03.03's drop off the roof: the walk holds on until a scene takes control.
    //  Looks: two for control, one as the leg starts, one per 250 ms slice.
    //  The arrow reads of a passing 03.04-03.11 run: 272 out of the gate, 254 after one turn.
};

//  A minimap arrow that reads `reads` in order, the last again and again;
//  none for no reads.
std::optional<MinimapArrowReader> scripted_arrow(const std::vector<double>& reads){
    if (reads.empty()){
        return std::nullopt;
    }
    auto next = std::make_shared<size_t>(0);
    return MinimapArrowReader([reads, next](){
        const double read = reads[std::min(*next, reads.size() - 1)];
        (*next)++;
        return read;
    });
}

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = plza_story_game(
        /*console=*/{}, builtin_story_data(), scripted_arrow(test.input.arrow_reads), sightings
    );
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
Pokemon::StoryGame<ZonePose> game_with_reload(const Pokemon::StoryRows& rows, AutosaveSightings& sightings){
    Pokemon::StoryGame<ZonePose> game = plza_story_game(/*console=*/{}, rows, /*arrow=*/std::nullopt, sightings);
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
        .name = "01.01's start is left to its first step's platform check",
        .input = {.checkpoint = "01.01", .looks = {CUTSCENE}},
        .expectation = {.verdict = "done", .inputs = "press HOME"},
    },
    {
        .name = "a game with no reload cannot load",
        .input = {.checkpoint = "01.01", .has_reload = false, .looks = {CUTSCENE}},
        .expectation = {.verdict = "no_reload", .inputs = ""},
    },
};

UnitTestResult check_load(const LoadCase& test, CancellableScope& scope){
    const StoryData& story = builtin_story_data();
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = test.input.has_reload
        ? game_with_reload(story, sightings)
        : plza_story_game(/*console=*/{}, story, /*arrow=*/std::nullopt, sightings);
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


//  Two rows: "01.01" waits for the platform, "01.02" runs `second_action`.
//  01.01's autosave shows when a look has both the platform and the badge;
//  its platform wait then needs two platform looks without the badge. The
//  badge look is 2 s after a first look (PLATFORM_SAVING): a badge in a
//  checkpoint's first 1.5 s is the save before it.
struct ChainCase{
    std::string name;
    struct Input{
        std::vector<Observation<ZonePose>> looks;
        //  The actions of 01.01's steps; a repeated one's step is named
        //  "<action>_again".
        std::vector<std::string> first_actions = {"wait_for_platform"};
        std::string second_action = "text_to_choice";
        size_t attempts = 1;
        //  01.02 reloads when chained; where the reload lands, and 01.02's
        //  start signature (none: its plan names none).
        bool reload_when_chained = false;
        StartSignature landed = {};
        std::optional<StartSignature> start = std::nullopt;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  How many times the reload pressed HOME.
        size_t reloads = 0;
        //  A log line the run wrote, if any is checked.
        std::string line = {};
        //  The run presses nothing after the reload's HOME.
        bool nothing_pressed_after_reload = false;
    };
    Expectation expectation;
};

const Observation<ZonePose> PLATFORM_SAVING = look({}, {PLATFORM_READING, AUTOSAVE_READING}, Milliseconds(2000));
const Observation<ZonePose> IN_CONTROL = look({ScreenEvent::PLAYER_CAN_MOVE});
//  01.01's two platform waits, the badge in the first.
const std::vector<Observation<ZonePose>> ROOF_SAVED = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM};
//  After a reload: START_LOOKS looks off the platform, then 01.02's scene (B at its box) to the party bar.
const std::vector<Observation<ZonePose>> AFTER_RELOAD = {
    CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, DIALOG,
    look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, Milliseconds(1000)), look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, Milliseconds(3000)),
};
const StartSignature STATION_FRONT{.talk_prompt = false, .party = 1};

const ChainCase CHAIN_CASES[] = {
    {
        .name = "autosave seen and the start check does not fail: on without a reload",
        .input = {.looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "no autosave seen and the start check does not fail: on without a reload",
        .input = {.looks = {PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "autosave seen and the start check fails: reloads the start",
        .input = {.looks = {
            CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0),
        }},
        .expectation = {.verdict = "done", .reloads = 1},
    },
    {
        .name = "no autosave seen and the start check fails: no reload, no save to start from",
        .input = {.looks = {PLATFORM}},
        .expectation = {.verdict = "failed at 01.02 chain: no_start_save", .reloads = 0},
    },
    {
        .name = "a reload onto an older save stops",
        .input = {.looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM}},
        .expectation = {.verdict = "failed at 01.02 chain: older_save", .reloads = 1},
    },
    {
        .name = "an autosave before the last step does not save the row: no save to reload the next from",
        .input = {
            .looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM},
            .first_actions = {"wait_for_platform", "wait_for_platform"},
        },
        .expectation = {
            .verdict = "failed at 01.02 chain: no_start_save", .reloads = 0,
            .line = "[Save] 01.01: the game autosaved in wait_for_platform, before wait_for_platform_again: not saved, a reload would skip wait_for_platform_again",
        },
    },
    {
        //  03.04's start save was its pokedex_app autosave, outside the Center.
        .name = "a run that autosaved and then failed does not reload: the save moved past its start",
        .input = {
            .looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, look(/*events=*/{}, {AUTOSAVE_READING}, Milliseconds(10000)), CUTSCENE},
            .second_action = "wait_for_platform",
            .attempts = 2,
        },
        .expectation = {
            .verdict = "failed at 01.02 : reload_failed", .reloads = 0,
            .line = "[Save] 01.02: the game autosaved in wait_for_platform before the run failed: the latest save is past 01.02's start",
        },
    },
    //  As 03.03 into 03.04: 01.01's badge shows in its first step (the roof's scene), not its last.
    {
        .name = "reload when chained: waits for control after the row before's last scene, reloads, lands at the start and runs",
        .input = {
            .looks = concat(ROOF_SAVED, concat({BLACK_FADE, BLACK_FADE, IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)),
            .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "mash_to_move_hints",
            .reload_when_chained = true, .landed = STATION_FRONT, .start = STATION_FRONT,
        },
        .expectation = {
            .verdict = "done", .reloads = 1,
            .line = "[Checkpoint] 01.02: control is back after 01.01's last scene; reloading the latest save",
        },
    },
    {
        .name = "reload when chained: the badge not seen at the row before's end is logged, not a condition",
        .input = {
            .looks = concat(ROOF_SAVED, concat({IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)),
            .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "mash_to_move_hints",
            .reload_when_chained = true, .landed = STATION_FRONT, .start = STATION_FRONT,
        },
        .expectation = {
            .verdict = "done", .reloads = 1,
            .line = "[Checkpoint] 01.02: reloads its start save when chained, whether or not 01.01's autosave showed at its end (not seen)",
        },
    },
    {
        .name = "reload when chained: control never comes back, no reload",
        .input = {
            .looks = concat(ROOF_SAVED, {BLACK_FADE}),
            .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "mash_to_move_hints",
            .reload_when_chained = true, .landed = STATION_FRONT, .start = STATION_FRONT,
        },
        .expectation = {.verdict = "failed at 01.02 chain: walk_not_free", .reloads = 0},
    },
    {
        .name = "reload when chained: the reload lands elsewhere, stops with nothing pressed",
        .input = {
            .looks = concat(ROOF_SAVED, concat({IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)),
            .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "mash_to_move_hints",
            .reload_when_chained = true, .landed = {.talk_prompt = true, .party = 1}, .start = STATION_FRONT,
        },
        .expectation = {
            .verdict = "failed at 01.02 chain: reload_off_start", .reloads = 1,
            .line = "[Checkpoint] 01.02: the reload landed somewhere other than its start: the screen reads (talk prompt, party 1), "
                "the start is (no talk prompt, party 1); nothing pressed after the reload",
            .nothing_pressed_after_reload = true,
        },
    },
    {
        .name = "reload when chained: no start signature to check the landing against, stops with nothing pressed",
        .input = {
            .looks = concat(ROOF_SAVED, concat({IN_CONTROL, IN_CONTROL}, AFTER_RELOAD)),
            .first_actions = {"wait_for_platform", "wait_for_platform"}, .second_action = "mash_to_move_hints",
            .reload_when_chained = true, .landed = STATION_FRONT,
        },
        .expectation = {.verdict = "failed at 01.02 chain: reload_unchecked", .reloads = 1, .nothing_pressed_after_reload = true},
    },
};

UnitTestResult check_chain(const ChainCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = game_with_reload(Pokemon::StoryRows{}, sightings);
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    StoryCheckpoint first = one_step_row("01.01", test.input.first_actions[0]);
    for (size_t index = 1; index < test.input.first_actions.size(); index++){
        const std::string& action = test.input.first_actions[index];
        first.steps.push_back(StoryStep{.kind = Pokemon::StoryStepKind::GAME, .name = std::format("{}_again", action), .why = "", .action = action, .args = {}});
    }
    const std::vector<StoryCheckpoint> rows{first, one_step_row("01.02", test.input.second_action)};
    std::vector<CheckpointStart> starts;
    if (test.input.start){
        starts.push_back(CheckpointStart{.checkpoint = "01.02", .signature = *test.input.start});
    }
    SegmentRunOptions options{.attempts_per_checkpoint = test.input.attempts, .starts = starts};
    options.read_start = [&test](const StartSignature&){
        return test.input.landed;
    };
    if (test.input.reload_when_chained){
        options.reload_when_chained = {"01.02"};
    }
    const Pokemon::SegmentReport report = run_plza_segments(game, rows, options, backend, state, sightings);
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {} {}: {}", report.failed_checkpoint, report.failed_event, report.failure.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const std::vector<std::string>& inputs = backend.inputs();
    const size_t reloads = (size_t)std::count(inputs.begin(), inputs.end(), "press HOME");
    if (reloads != test.expectation.reloads){
        return std::format("expected {} reloads, got {}: {}", test.expectation.reloads, reloads, joined(inputs, ", "));
    }
    const auto home = std::find(inputs.rbegin(), inputs.rend(), "press HOME");
    if (test.expectation.nothing_pressed_after_reload && std::any_of(inputs.rbegin(), home, [](const std::string& in){ return in.starts_with("press"); })){
        return std::format("expected no press after the reload: {}", joined(inputs, ", "));
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

class AutosaveStepTest : public UnitTest{
public:
    explicit AutosaveStepTest(AutosaveStepCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame autosave step - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_autosave_step(m_case);
    }
private:
    AutosaveStepCase m_case;
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

//  A run of built-in rows, then 01.01's walk off the platform, the
//  console's new game steps scripted to pass, on a game with no reload: the
//  choices row hands over to Confirm Choices, which hands over to the
//  platform, with no save loaded between them.
struct NewGameChainCase{
    std::string name;
    struct Input{
        std::vector<std::string> rows;
        std::vector<Observation<ZonePose>> looks;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  The console new game steps run, in order.
        std::string console_steps;
    };
    Expectation expectation;
};

const NewGameChainCase NEW_GAME_CHAIN_CASES[] = {
    {
        .name = "01.00a, 01.00b and 01.01 in one run",
        .input = {.rows = {"01.00a", "01.00b"}, .looks = concat({CUTSCENE, OWN_AUTOSAVE, PLATFORM, PLATFORM}, walk_into_white_fade())},
        .expectation = {.verdict = "done", .console_steps = "new_game_intro confirm_new_game"},
    },
    {
        .name = "01.00b and 01.01 in one run",
        .input = {.rows = {"01.00b"}, .looks = concat({CUTSCENE, OWN_AUTOSAVE, PLATFORM, PLATFORM}, walk_into_white_fade())},
        .expectation = {.verdict = "done", .console_steps = "confirm_new_game"},
    },
};

UnitTestResult check_new_game_chain(const NewGameChainCase& test, CancellableScope& scope){
    const StoryData& story = builtin_story_data();
    std::vector<std::string> steps_run;
    PlzaConsoleActions console;
    for (const std::string_view action : {"new_game_intro", "confirm_new_game"}){
        console.emplace(std::string(action), [&steps_run, action](Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&){
            steps_run.emplace_back(action);
            return Pokemon::outcome_ok();
        });
    }
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = plza_story_game(console, story, /*arrow=*/std::nullopt, sightings);
    std::vector<StoryCheckpoint> rows;
    for (const std::string& id : test.input.rows){
        rows.push_back(builtin_row(id, /*first_step=*/0, /*steps=*/std::numeric_limits<size_t>::max()));
    }
    rows.push_back(builtin_row("01.01", /*first_step=*/0, /*steps=*/2));
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks, .kind = Pokemon::StoryBackendKind::CONSOLE}, scope);
    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_plza_segments(game, rows, /*options=*/{}, backend, state, sightings);
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {} {}: {}", report.failed_checkpoint, report.failed_event, report.failure.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const std::string console_steps = joined(steps_run, " ");
    if (console_steps != test.expectation.console_steps){
        return std::format("expected the console steps \"{}\", got \"{}\"", test.expectation.console_steps, console_steps);
    }
    return true;
}

class NewGameChainTest : public UnitTest{
public:
    explicit NewGameChainTest(NewGameChainCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame new game chain - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_new_game_chain(m_case, scope);
    }
private:
    NewGameChainCase m_case;
};

//  A checkpoint whose last step saves from the menu.
struct MenuSaveCase{
    std::string name;
    struct Input{
        //  The last step's action.
        std::string action;
        //  Whether that step succeeds.
        bool succeeds = true;
    };
    Input input;
    struct Expectation{
        //  The checkpoint's save: "saved", or its failure's detail.
        std::string save;
    };
    Expectation expectation;
};

const MenuSaveCase MENU_SAVE_CASES[] = {
    {
        .name = "a menu save that succeeds saves the checkpoint",
        .input = {.action = std::string(MENU_SAVE_ACTION)},
        .expectation = {.save = "saved"},
    },
    {
        .name = "a menu save that fails saves nothing",
        .input = {.action = std::string(MENU_SAVE_ACTION), .succeeds = false},
        .expectation = {.save = "no autosave seen"},
    },
    {
        .name = "another step that succeeds saves nothing",
        .input = {.action = "walk_to_unread_marker"},
        .expectation = {.save = "no autosave seen"},
    },
};

UnitTestResult check_menu_save(const MenuSaveCase& test, CancellableScope& scope){
    const bool succeeds = test.input.succeeds;
    Pokemon::StoryGame<ZonePose> inner;
    inner.actions.push_back(Pokemon::StoryAction<ZonePose>{
        .spec = {.kind = Pokemon::StoryStepKind::GAME, .name = test.input.action},
        .handler = [succeeds](Pokemon::StoryBackend<ZonePose>&, const StoryActionCall&, Pokemon::RunState&){
            return succeeds ? Pokemon::outcome_ok() : Pokemon::outcome_failed("save_failed", "no save dialog");
        },
    });
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = with_autosave_sightings(inner, Pokemon::StoryRows{}, sightings);
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = {}}, scope);
    const StoryCheckpoint row = one_step_row("T", test.input.action);
    const Pokemon::StoryActionArgs args;
    Pokemon::RunState state;
    game.actions.front().handler(backend, StoryActionCall{.step = row.steps.front(), .args = args}, state);
    const Pokemon::Outcome saved = game.save(backend, row);
    const std::string save = saved.succeeded() ? "saved" : saved.detail;
    if (save != test.expectation.save){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.save, save, joined(backend.lines(), "\n"));
    }
    return true;
}

class MenuSaveTest : public UnitTest{
public:
    explicit MenuSaveTest(MenuSaveCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame menu save - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_menu_save(m_case, scope);
    }
private:
    MenuSaveCase m_case;
};

//  One look AutosaveSightings notes, or the start of a checkpoint.
struct SightingLook{
    //  The step the look is taken in; empty: a checkpoint starts
    //  (AutosaveSightings::restart()).
    std::string step;
    bool badge = false;
    Milliseconds at = Milliseconds(0);
};

struct SightingsCase{
    std::string name;
    struct Input{
        std::vector<SightingLook> looks;
    };
    Input input;
    struct Expectation{
        //  AutosaveSightings::seen_in() after the looks.
        std::optional<std::string> seen_in;
    };
    Expectation expectation;
};

const SightingsCase SIGHTINGS_CASES[] = {
    {
        //  02.04's check-in autosave still up 1.2 s into 02.05 is 02.04's.
        .name = "a badge still up from the last checkpoint's autosave is not this checkpoint's",
        .input = {.looks = {
            {.step = "check_in", .badge = true, .at = Milliseconds(93500)},
            {.step = ""},
            {.step = "to_lobby_elevator", .badge = true, .at = Milliseconds(94700)},
            {.step = "to_lobby_elevator", .badge = true, .at = Milliseconds(95000)},
            {.step = "to_lobby_elevator", .badge = false, .at = Milliseconds(95500)},
            {.step = "take_elevator_up", .badge = false, .at = Milliseconds(106000)},
        }},
        .expectation = {.seen_in = std::nullopt},
    },
    {
        .name = "a new badge after the last checkpoint's has gone is this checkpoint's",
        .input = {.looks = {
            {.step = "check_in", .badge = true, .at = Milliseconds(93500)},
            {.step = ""},
            {.step = "to_lobby_elevator", .badge = true, .at = Milliseconds(94700)},
            {.step = "to_lobby_elevator", .badge = false, .at = Milliseconds(95500)},
            {.step = "take_elevator_up", .badge = true, .at = Milliseconds(106000)},
        }},
        .expectation = {.seen_in = "take_elevator_up"},
    },
    {
        //  02.07's autosave still up 37 ms into 03.01 is 02.07's.
        .name = "a badge on a checkpoint's first look with none seen before it is the last checkpoint's save",
        .input = {.looks = {
            {.step = "first_evening", .badge = false, .at = Milliseconds(0)},
            {.step = ""},
            {.step = "out_of_room_202", .badge = true, .at = Milliseconds(1300)},
            {.step = "out_of_room_202", .badge = true, .at = Milliseconds(1700)},
            {.step = "out_of_room_202", .badge = false, .at = Milliseconds(3000)},
            {.step = "leave_room_202", .badge = false, .at = Milliseconds(9000)},
        }},
        .expectation = {.seen_in = std::nullopt},
    },
    {
        .name = "a badge first seen past the checkpoint's first moments is this checkpoint's",
        .input = {.looks = {
            {.step = ""},
            {.step = "opening", .badge = false, .at = Milliseconds(0)},
            {.step = "opening", .badge = true, .at = Milliseconds(2000)},
        }},
        .expectation = {.seen_in = "opening"},
    },
};

UnitTestResult check_sightings(const SightingsCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = {}}, scope);
    for (const SightingLook& entry : test.input.looks){
        if (entry.step.empty()){
            sightings.finish("");
            sightings.restart();
            continue;
        }
        sightings.note(backend, look(/*events=*/{}, entry.badge ? std::vector<std::string_view>{AUTOSAVE_READING} : std::vector<std::string_view>{}, entry.at), entry.step);
    }
    if (sightings.seen_in() != test.expectation.seen_in){
        return std::format(
            "expected seen in \"{}\", got \"{}\"; log:\n{}",
            test.expectation.seen_in.value_or("none"), sightings.seen_in().value_or("none"), joined(backend.lines(), "\n")
        );
    }
    return true;
}

class SightingsTest : public UnitTest{
public:
    explicit SightingsTest(SightingsCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame autosave sightings - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_sightings(m_case, scope);
    }
private:
    SightingsCase m_case;
};

} // namespace


void add_tests_AutoStoryGame(UnitTestDatabase& database){
    for (const SightingsCase& test : SIGHTINGS_CASES){
        database.add<SightingsTest>(test);
    }
    for (const MenuSaveCase& test : MENU_SAVE_CASES){
        database.add<MenuSaveTest>(test);
    }
    for (const StoryDataCase& test : STORY_DATA_CASES){
        database.add<StoryDataTest>(test);
    }
    for (const LintCase& test : LINT_CASES){
        database.add<LintTest>(test);
    }
    for (const AutosaveStepCase& test : AUTOSAVE_STEP_CASES){
        database.add<AutosaveStepTest>(test);
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
    for (const NewGameChainCase& test : NEW_GAME_CHAIN_CASES){
        database.add<NewGameChainTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
