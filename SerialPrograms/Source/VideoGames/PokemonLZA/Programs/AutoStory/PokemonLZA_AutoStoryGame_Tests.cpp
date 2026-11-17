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
        //  "<id>:<new_game|from_save>[:<autosave|unverified>]" per row, in order.
        std::string rows;
    };
    Expectation expectation;
};

const StoryDataCase STORY_DATA_CASES[] = {
    {
        .name = "the built-in story loads and passes the lint",
        .input = {},
        .expectation = {.rows =
            "01.00:new_game:autosave 01.01:from_save:unverified 01.02:from_save:unverified 01.03:from_save:unverified "
            "02.01:from_save:unverified 02.02:from_save:unverified 02.03:from_save:unverified 02.04:from_save:unverified "
            "02.05:from_save:unverified 02.06:from_save:autosave 02.07:from_save:unverified "
            "03.01:from_save:unverified 03.02:from_save:autosave 03.02b:from_save 03.03:from_save:autosave 03.04:from_save:autosave "
            "03.05:from_save:autosave 03.06:from_save:autosave 03.07:from_save:autosave 03.08:from_save:autosave 03.08b:from_save:autosave "
            "03.09:from_save:autosave "
            "03.10:from_save:autosave 03.11:from_save:autosave"
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
        .name = "a walk to an unknown end fails",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
            {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(4500)}}}, {"end", std::string("sideways")},
        }}},
        .expectation = {.errors = "01.01 w: \"end\" must be \"fade_white\", \"fade_black\", \"dialog\" or \"prompt\""},
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
        .name = "a talk after a walk to its A prompt passes",
        .input = {
            .before = {{.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
                {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(3000)}}}, {"end", std::string("prompt")},
            }}},
            .step = {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
        },
        .expectation = {.errors = ""},
    },
    {
        .name = "a talk after walk_to_prompt passes",
        .input = {
            .before = {{.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk_to_prompt", .args = {}}},
            .step = {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
        },
        .expectation = {.errors = ""},
    },
    {
        //  console B, 2026-11-16: 03.02's timed walk to Taunie stopped
        //  short of her, and the talk's A found no one.
        .name = "a talk after a timed walk fails",
        .input = {
            .before = {{.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk", .args = {
                {"legs", std::vector<Pokemon::StoryTimedLeg>{{.turn = 0, .walk = Milliseconds(3000)}}},
            }}},
            .step = {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
        },
        .expectation = {.errors = "01.01 t: a talk must follow a step that ends on its A prompt (a walk to \"end\": \"prompt\", or walk_to_prompt)"},
    },
    {
        .name = "a story battle after story_battle_start passes",
        .input = {
            .before = {{.kind = Pokemon::StoryStepKind::GAME, .name = "s", .why = "", .action = "story_battle_start", .args = {}}},
            .step = {.kind = Pokemon::StoryStepKind::GAME, .name = "b", .why = "", .action = "story_battle", .args = {}},
        },
        .expectation = {.errors = ""},
    },
    {
        //  console B, 2026-11-16 (03.02b run 2): the battle ran right after
        //  the talk, with no battle on screen.
        .name = "a story battle right after a talk fails",
        .input = {
            .before = {
                {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk_to_prompt", .args = {}},
                {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
            },
            .step = {.kind = Pokemon::StoryStepKind::GAME, .name = "b", .why = "", .action = "story_battle", .args = {}},
        },
        .expectation = {.errors = "01.01 b: a story battle must follow story_battle_start, which ends on the battle HUD"},
    },
    {
        .name = "a scene after a talk passes",
        .input = {
            .before = {
                {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk_to_prompt", .args = {}},
                {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
            },
            .step = {.kind = Pokemon::StoryStepKind::MASH, .name = "s", .why = "", .action = "a", .args = {}},
        },
        .expectation = {.errors = ""},
    },
    {
        //  console B, 2026-11-16: 03.04's talk_to_pokedex_screen played the
        //  nurse's wrap-up and Taunie's scene to control, and the scene step
        //  after it waited 45 s on the party bar for a scene already played.
        .name = "a scene after talk_to_pokedex_screen fails",
        .input = {
            .before = {{.kind = Pokemon::StoryStepKind::GAME, .name = "p", .why = "", .action = "talk_to_pokedex_screen", .args = {}}},
            .step = {.kind = Pokemon::StoryStepKind::MASH, .name = "s", .why = "", .action = "a", .args = {}},
        },
        .expectation = {.errors = "01.01 s: a scene must follow what starts it, not talk_to_pokedex_screen, which plays its scenes through to control"},
    },
    {
        .name = "a scene after a scene fails",
        .input = {
            .before = {
                {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk_to_prompt", .args = {}},
                {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
                {.kind = Pokemon::StoryStepKind::MASH, .name = "s1", .why = "", .action = "a", .args = {}},
            },
            .step = {.kind = Pokemon::StoryStepKind::MASH, .name = "s2", .why = "", .action = "a", .args = {}},
        },
        .expectation = {.errors = "01.01 s2: a scene must follow what starts it, not a, which plays its scenes through to control"},
    },
    {
        //  plza/autostory ran 03.09's five_registered this way: control at
        //  once after research_lab_gate's scene (console B, 2026-11-14 21:01).
        .name = "a bare scene after a scene passes: control ends it at once",
        .input = {
            .before = {
                {.kind = Pokemon::StoryStepKind::GAME, .name = "w", .why = "", .action = "walk_to_prompt", .args = {}},
                {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}},
                {.kind = Pokemon::StoryStepKind::MASH, .name = "s1", .why = "", .action = "a", .args = {}},
            },
            .step = {.kind = Pokemon::StoryStepKind::MASH, .name = "s2", .why = "", .action = "", .args = {}},
        },
        .expectation = {.errors = ""},
    },
    {
        .name = "a checkpoint's first step cannot be a talk",
        .input = {.step = {.kind = Pokemon::StoryStepKind::TALK, .name = "t", .why = "", .action = "", .args = {}}},
        .expectation = {.errors = "01.01 t: a talk must follow a step that ends on its A prompt (a walk to \"end\": \"prompt\", or walk_to_prompt)"},
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
        //  console B, 2026-11-16: 03.06 autosaved in wild_zones_tutorial and
        //  went on to the gate; 03.07 reloaded that save and threw outside
        //  Wild Zone 1.
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
const Observation<ZonePose> CUTSCENE = look({});
const Observation<ZonePose> TEXT_FOR_A = look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING});
const Observation<ZonePose> STILL = look({ScreenEvent::SCREEN_STILL});
const Observation<ZonePose> WHITE_FADE = look({ScreenEvent::FADE_WHITE});
const Observation<ZonePose> BLACK_FADE = look({ScreenEvent::FADE_BLACK});
const Observation<ZonePose> DIALOG = look({ScreenEvent::DIALOG_BOX});
const Observation<ZonePose> BATTLE_HUD = look({ScreenEvent::BATTLE_STARTED});
const Observation<ZonePose> A_PROMPT = look({ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE});

//  The party bar (free control), `at` after the scripts' common start.
Observation<ZonePose> party_bar(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, at);
}

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
        .name = "01.00 stops at the new game off a console",
        .input = {.checkpoint = "01.00", .first_step = 0, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at new_game: console_only", .inputs = ""},
    },
    {
        .name = "01.00 stops at Confirm Choices off a console",
        .input = {.checkpoint = "01.00", .first_step = 1, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at confirm: console_only", .inputs = ""},
    },
    {
        .name = "01.00's opening ends on the platform after the autosave",
        .input = {.checkpoint = "01.00", .first_step = 2, .looks = {CUTSCENE, PLATFORM, AUTOSAVE, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 300ms, wait 300ms, wait 300ms, wait 300ms, wait 300ms",
            .line = "[PLZA] platform wait: on the platform after 6 looks",
        },
    },
    {
        .name = "01.00's opening presses A once for the text waiting for it",
        .input = {.checkpoint = "01.00", .first_step = 2, .looks = {TEXT_FOR_A, TEXT_FOR_A, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .inputs = "press A, wait 300ms, wait 300ms, wait 300ms, wait 300ms"},
    },
    {
        .name = "01.00's opening fails with no autosave",
        .input = {.checkpoint = "01.00", .first_step = 2, .looks = {PLATFORM}},
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
        //  console B, 2026-11-15 (English and Italian): after the ad, the
        //  loading screen fades from black into the rival's talk, a second
        //  black fade, Pancham; the mission banner (which the detector can
        //  miss) comes before a third black fade, then control. 5 s quiet
        //  after the banner, past the second fade, is not the end.
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
        .name = "01.00's autosave badge is logged in the step it shows in",
        .input = {.checkpoint = "01.00", .first_step = 2, .looks = {CUTSCENE, AUTOSAVE, AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .line = "[PLZA] autosave badge at 0.0s in opening"},
    },
    {
        .name = "01.00 ends on its autosave",
        .input = {.checkpoint = "01.00", .first_step = 2, .looks = {AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .line = "[Save] 01.00: the game autosaved (seen)"},
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
        .name = "01.03 walks to Chikorita: a turn left, then on to its A prompt",
        .input = {.checkpoint = "01.03", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(5, look(/*events=*/{}, {CONTROLS_HINT_READING})), {A_PROMPT}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, press L, stick -70deg 150ms walk, release 100ms, press L, hold north 250ms walk, hold north 250ms walk, wait 300ms",
            .line = "[PLZA] approach_starter: the end event after 500 ms of the last leg.",
        },
    },
    {
        //  console B, 2026-11-16 22:47: with no party there is no party
        //  bar, and the walk waited 10 s of quiet on the starter's prompt.
        .name = "01.03's walk to Chikorita starts on the A prompt held a second with no party bar",
        .input = {.checkpoint = "01.03", .first_step = 0, .steps = 1, .looks = {look({ScreenEvent::INTERACT_PROMPT})}},
        .expectation = {
            .verdict = "done",
            .line = "[PLZA] approach_starter: the A prompt held 1000 ms with nothing busy: control.",
        },
    },
    {
        .name = "01.03's walk to Chikorita fails with no A prompt in its 750 ms",
        .input = {.checkpoint = "01.03", .first_step = 0, .steps = 1, .looks = {look(/*events=*/{}, {CONTROLS_HINT_READING})}},
        .expectation = {.verdict = "failed at approach_starter: walk_no_end"},
    },
    {
        .name = "01.03's talk to the starter: A at its prompt, then the text box",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {A_PROMPT, A_PROMPT, DIALOG}},
        .expectation = {.verdict = "done", .inputs = "wait 250ms, press A, wait 250ms"},
    },
    {
        .name = "01.03's talk fails when nothing starts after A: the prompt stays",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {A_PROMPT}},
        .expectation = {.verdict = "failed at talk_to_starter: talk_no_scene"},
    },
    {
        //  console B, 2026-11-16: 02.05's A at the elevator was lost and
        //  the next step read the overworld for 120 s.
        .name = "a talk presses A again while the A prompt stays with nothing open",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(7, A_PROMPT), {DIALOG}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = std::format("wait 250ms, press A, {}, press A, wait 250ms", repeated("wait 250ms", 4)),
            .line = "[PLZA] talk: the A prompt still up 1000 ms after A 1 with nothing open; A again",
        },
    },
    {
        .name = "a talk presses nothing on an A prompt seen on one look only",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {A_PROMPT, CUTSCENE}},
        .expectation = {.verdict = "failed at talk_to_starter: talk_no_prompt", .inputs = repeated("wait 250ms", 12)},
    },
    {
        //  console B, 2026-11-16 23:11 (German, 02.05): the walk to the
        //  elevator ended on a first glimpse of its prompt, the talk gave
        //  up after 1 s with none, and "A Aufzug nehmen" stood steady a few
        //  seconds later.
        .name = "a talk waits for an A prompt that comes up 2 s in, then presses A",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(8, party_bar(Milliseconds(0))), {A_PROMPT, A_PROMPT, DIALOG}
        )},
        .expectation = {.verdict = "done", .inputs = std::format("{}, press A, wait 250ms", repeated("wait 250ms", 9))},
    },
    {
        //  console B, 2026-11-16 (03.02b run 2): the talk counted as
        //  started 0.4 s after A, and no battle followed.
        .name = "a talk does not start on one moving look that the party bar follows",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {A_PROMPT, A_PROMPT, CUTSCENE, party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at talk_to_starter: talk_no_scene"},
    },
    {
        .name = "a talk fails when the party bar stays after A",
        .input = {.checkpoint = "01.03", .first_step = 1, .steps = 1, .looks = {A_PROMPT, A_PROMPT, party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at talk_to_starter: talk_no_scene"},
    },
    {
        .name = "01.03's battle start: A through the talk and the tutorial card, done on the battle HUD",
        .input = {.checkpoint = "01.03", .first_step = 2, .steps = 1, .looks = {
            look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, {TUTORIAL_CARD_READING}, Milliseconds(500)),
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
        //  console B, 2026-11-16 (03.02b run 2): the battle code looked for
        //  a battle for 2 min on the overworld.
        .name = "a story battle with no battle HUD in 10 s fails as battle_never_started, the battle code not run",
        .input = {.checkpoint = "01.03", .first_step = 3, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at andi_battle: battle_never_started", .inputs = repeated("wait 250ms", 40)},
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
            .inputs = "press B, press A, wait 250ms",
            .line = "[Save] 01.03: not saved (save unverified)",
        },
    },
    {
        .name = "02.01 waits 3 s for the NPCs, then holds its legs",
        .input = {.checkpoint = "02.01", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 3000ms, wait 250ms, stick 10deg 4250ms walk, press L, stick 90deg 6000ms walk, press L, stick 90deg 6500ms walk",
        },
    },
    {
        .name = "02.01's scene after Brigitte: B at the text, done on the party bar held a second, not saved",
        .input = {.checkpoint = "02.01", .first_step = 3, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)), party_bar(Milliseconds(500)), party_bar(Milliseconds(1600)),
        }},
        .expectation = {.verdict = "done", .inputs = "press B, wait 250ms", .line = "[Save] 02.01: not saved (save unverified)"},
    },
    {
        //  console B, 2026-11-16 20:42: mash_to_move_hints pressed nothing on
        //  Mable's mission and reward menus for 60 s; plza/autostory's bare
        //  scene pressed B on every look and passed (2026-11-14 22:17).
        .name = "03.11's leave_mable: B on every look through Mable's menus, done on the party bar held a second",
        .input = {.checkpoint = "03.11", .first_step = 3, .steps = 1, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)), look(/*events=*/{}, /*readings=*/{}, Milliseconds(500)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)), party_bar(Milliseconds(1500)), party_bar(Milliseconds(2600)),
        }},
        .expectation = {.verdict = "done", .inputs = "press B, press B, press B, wait 250ms"},
    },
    {
        .name = "03.11's leave_mable: a choice and a tutorial card still get A between the B presses",
        .input = {.checkpoint = "03.11", .first_step = 3, .steps = 1, .looks = {
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(0)), look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, /*readings=*/{}, Milliseconds(500)),
            look(/*events=*/{}, {TUTORIAL_CARD_READING}, Milliseconds(1000)), party_bar(Milliseconds(1500)), party_bar(Milliseconds(2600)),
        }},
        .expectation = {.verdict = "done", .inputs = "press B, press A, press A, wait 250ms"},
    },
    {
        .name = "03.09's five_registered: done at once on control after research_lab_gate's scene",
        .input = {.checkpoint = "03.09", .first_step = 2, .steps = 1, .looks = {party_bar(Milliseconds(0)), party_bar(Milliseconds(1100))}},
        .expectation = {.verdict = "done", .inputs = "wait 250ms"},
    },
    {
        //  console B, 2026-11-13: 02.02 after the Brigitte battle walked
        //  past the street scene's trigger and A-mashed for two minutes.
        .name = "02.02's street scene fails when the party bar shows 45 s and it never starts",
        .input = {.checkpoint = "02.02", .first_step = 1, .steps = 1, .looks = {party_bar(Milliseconds(0)), party_bar(Milliseconds(46000))}},
        .expectation = {.verdict = "failed at street_scene: scene_never_started", .inputs = "wait 250ms"},
    },
    {
        .name = "02.02 walks on to the gate's A prompt",
        .input = {.checkpoint = "02.02", .first_step = 2, .steps = 1, .looks = {CUTSCENE, CUTSCENE, A_PROMPT}},
        .expectation = {
            .verdict = "done",
            .inputs = "hold north 250ms walk, hold north 250ms walk, wait 300ms",
            .line = "[PLZA] walk_to_prompt: the A prompt shows after 500 ms of walking",
        },
    },
    {
        .name = "walk_to_prompt walks on past an A prompt gone again 300 ms later",
        .input = {.checkpoint = "02.02", .first_step = 2, .steps = 1, .looks = {CUTSCENE, A_PROMPT, CUTSCENE, A_PROMPT}},
        .expectation = {
            .verdict = "done",
            .inputs = "hold north 250ms walk, wait 300ms, hold north 250ms walk, wait 300ms",
            .line = "[PLZA] walk_to_prompt: the A prompt shows after 500 ms of walking",
        },
    },
    {
        .name = "02.02's walk to the gate fails with no A prompt",
        .input = {.checkpoint = "02.02", .first_step = 2, .steps = 1, .looks = {CUTSCENE}},
        .expectation = {.verdict = "failed at to_battle_zone_gate: no_prompt"},
    },
    {
        .name = "02.03's lobby: A at the door's prompt until the fade, then the scene to the party bar",
        .input = {.checkpoint = "02.03", .first_step = 1, .looks = {
            A_PROMPT, BLACK_FADE,
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(1000)), party_bar(Milliseconds(1500)), party_bar(Milliseconds(2600)),
        }},
        .expectation = {.verdict = "done", .inputs = "press A, wait 250ms, press B, wait 250ms"},
    },
    {
        .name = "02.04's walk to the clerk: along the counter, then the turn to her A prompt",
        .input = {.checkpoint = "02.04", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(6, party_bar(Milliseconds(0))), {A_PROMPT}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, press L, stick -5deg 150ms walk, release 100ms, press L, hold north 2500ms walk, "
                "press L, stick -85deg 150ms walk, release 100ms, press L",
            .line = "[PLZA] to_front_counter: the end event after the last turn.",
        },
    },
    {
        .name = "02.04's walk to the clerk fails with no A prompt after its last turn",
        .input = {.checkpoint = "02.04", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at to_front_counter: walk_no_end"},
    },
    {
        .name = "02.05's elevator: A through its line to the floor choice",
        .input = {.checkpoint = "02.05", .first_step = 2, .steps = 1, .looks = {DIALOG, choice(0, 3)}},
        .expectation = {.verdict = "done", .inputs = "press A"},
    },
    {
        .name = "02.05's walk to room 202 starts after 10 s with nothing busy: the hall shows no party bar",
        .input = {.checkpoint = "02.05", .first_step = 5, .steps = 1, .looks = {CUTSCENE}},
        .expectation = {
            .verdict = "done",
            .inputs = repeated("wait 250ms", 40) + ", press L, stick -15deg 150ms walk, release 100ms, press L, hold north 2000ms walk",
            .line = "[PLZA] to_room_202: nothing busy for 10000 ms and no sign of control; taken as control.",
        },
    },
    {
        .name = "02.05's ride to 2F: the hall's quiet is control",
        .input = {.checkpoint = "02.05", .first_step = 4, .steps = 1, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)), look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::SCREEN_STILL}, /*readings=*/{}, Milliseconds(6000)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press B, wait 500ms",
            .line = "[PLZA mash_to_move_hints] done: quiet, taken as control.",
        },
    },
    {
        .name = "02.06 holds the stick at 45 degrees to the bed's A prompt",
        .input = {.checkpoint = "02.06", .first_step = 0, .steps = 1, .looks = {
            party_bar(Milliseconds(0)), party_bar(Milliseconds(0)), party_bar(Milliseconds(0)), A_PROMPT,
        }},
        .expectation = {.verdict = "done", .inputs = std::format("wait 250ms, {}, wait 300ms", repeated("stick 45deg 250ms walk", 3))},
    },
    {
        .name = "02.06 with no autosave badge seen is not saved",
        .input = {.checkpoint = "02.06", .first_step = 4, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)), look({ScreenEvent::SCREEN_STILL}, /*readings=*/{}, Milliseconds(21000)),
        }},
        .expectation = {.verdict = "done", .line = "[Save] 02.06: not saved (the plan's autosave was not seen)"},
    },
    {
        .name = "an autosave badge mid-scene saves the checkpoint",
        .input = {.checkpoint = "02.06", .first_step = 4, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, {AUTOSAVE_READING}, Milliseconds(1000)),
            look({ScreenEvent::SCREEN_STILL}, /*readings=*/{}, Milliseconds(21000)),
        }},
        .expectation = {.verdict = "done", .line = "[Save] 02.06: the game autosaved (seen)"},
    },
    {
        .name = "a badge mid-scene is logged once, at its time in its step",
        .input = {.checkpoint = "02.06", .first_step = 4, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look(/*events=*/{}, {AUTOSAVE_READING}, Milliseconds(1000)),
            look(/*events=*/{}, {AUTOSAVE_READING}, Milliseconds(2000)),
            look({ScreenEvent::SCREEN_STILL}, /*readings=*/{}, Milliseconds(21000)),
        }},
        .expectation = {.verdict = "done", .line = "[PLZA] autosave badge at 1.0s in wake_up"},
    },
    {
        .name = "03.02b walks on to the plaza scene's A prompt",
        .input = {.checkpoint = "03.02b", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(4, party_bar(Milliseconds(0))), {A_PROMPT}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, hold north 250ms walk, hold north 250ms walk, wait 300ms",
            .line = "[PLZA] to_plaza_scene_prompt: the end event after 500 ms of the last leg.",
        },
    },
    {
        //  console B, 2026-11-16 23:11 (German, 02.05): the walk to the
        //  elevator ended on a first glimpse of "A Aufzug nehmen", and the
        //  talk after it found no prompt.
        .name = "a walk to an A prompt holds on past a glimpse gone again 300 ms later",
        .input = {.checkpoint = "03.02b", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(4, party_bar(Milliseconds(0))), {A_PROMPT, party_bar(Milliseconds(0)), A_PROMPT}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, hold north 250ms walk, hold north 250ms walk, wait 300ms, hold north 250ms walk, wait 300ms",
            .line = "[PLZA] to_plaza_scene_prompt: the end event after 750 ms of the last leg.",
        },
    },
    {
        //  console B, 2026-11-16 23:48 (German, 02.05, run 14): 750 ms into
        //  the walk to the elevator, NPC Alton's "A Ansprechen" showed
        //  beside the path and stood steady; the walk ended there, short of
        //  the door, and the talk opened Alton's line. The elevator's
        //  prompt shows at the door, about 2 s in.
        .name = "02.05's walk to the elevator walks past a prompt before its measured time to the one at the door",
        .input = {.checkpoint = "02.05", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<Observation<ZonePose>>(4, party_bar(Milliseconds(0))),
            concat({A_PROMPT, A_PROMPT}, concat(std::vector<Observation<ZonePose>>(3, party_bar(Milliseconds(0))), {A_PROMPT}))
        )},
        .expectation = {
            .verdict = "done",
            .inputs = std::format("wait 250ms, {}, wait 300ms", repeated("hold north 250ms walk", 8)),
            .line = "[PLZA] to_lobby_elevator: the end event after 2000 ms of the last leg.",
        },
    },
    {
        .name = "03.02b's walk to the plaza scene fails with no A prompt within its bound",
        .input = {.checkpoint = "03.02b", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at to_plaza_scene_prompt: walk_no_end"},
    },
    {
        //  console B, 2026-11-16 (run 3): the first prompt from the reload
        //  spot starts a scene (a fade, the Smart Rotom card, a fade) that
        //  hands control back at the arch, Taunie a few steps ahead.
        .name = "03.02b's plaza scene: A at its prompt, through the fades, done on the party bar held a second",
        .input = {.checkpoint = "03.02b", .first_step = 1, .steps = 1, .looks = {
            A_PROMPT, BLACK_FADE,
            look({ScreenEvent::SCREEN_STILL}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(11200)),
            party_bar(Milliseconds(11800)), party_bar(Milliseconds(12900)),
        }},
        .expectation = {.verdict = "done", .line = "[PLZA mash_to_move_hints] done on can_move."},
    },
    {
        .name = "03.02b walks on from the arch to Taunie's A prompt",
        .input = {.checkpoint = "03.02b", .first_step = 2, .steps = 1, .looks = {party_bar(Milliseconds(0)), party_bar(Milliseconds(0)), A_PROMPT}},
        .expectation = {.verdict = "done", .inputs = "hold north 250ms walk, hold north 250ms walk, wait 300ms"},
    },
    {
        //  console B, 2026-11-16: the walk stopped short of Taunie, under
        //  the Quartiere Verde arch, and the talk's A found no one.
        .name = "03.02b's talk presses nothing without Taunie's A prompt",
        .input = {.checkpoint = "03.02b", .first_step = 3, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {
            .verdict = "failed at talk_to_taunie: talk_no_prompt",
            .inputs = repeated("wait 250ms", 12),
        },
    },
    {
        //  console B, 2026-11-16: A at Taunie dimmed the screen into a
        //  "Smart Rotom" card, with no text box: her talk had started.
        .name = "03.02b's talk to Taunie starts on a tutorial card with no text box",
        .input = {.checkpoint = "03.02b", .first_step = 3, .steps = 1, .looks = {
            A_PROMPT, A_PROMPT, look({ScreenEvent::SCREEN_STILL}, {TUTORIAL_CARD_READING}),
        }},
        .expectation = {.verdict = "done", .inputs = "wait 250ms, press A, wait 250ms"},
    },
    {
        .name = "03.02b's talk to Taunie starts on a subtitle: the advance chevron with no text box",
        .input = {.checkpoint = "03.02b", .first_step = 3, .steps = 1, .looks = {
            A_PROMPT, A_PROMPT, look({ScreenEvent::SCREEN_STILL}, {ADVANCE_ARROW_READING}),
        }},
        .expectation = {.verdict = "done", .inputs = "wait 250ms, press A, wait 250ms"},
    },
    {
        .name = "03.02b's talk to Taunie starts on a fade",
        .input = {.checkpoint = "03.02b", .first_step = 3, .steps = 1, .looks = {A_PROMPT, A_PROMPT, BLACK_FADE}},
        .expectation = {.verdict = "done", .inputs = "wait 250ms, press A, wait 250ms"},
    },
    {
        .name = "03.02b's talk to Taunie starts on a textless cutscene: the screen moving with no HUD",
        .input = {.checkpoint = "03.02b", .first_step = 3, .steps = 1, .looks = {A_PROMPT, A_PROMPT, CUTSCENE}},
        .expectation = {.verdict = "done", .inputs = "wait 250ms, press A, wait 250ms"},
    },
    {
        .name = "03.02b's battle start: A through Taunie's subtitles and the card, done on the battle HUD",
        .input = {.checkpoint = "03.02b", .first_step = 4, .steps = 1, .looks = {
            look(/*events=*/{}, {ADVANCE_ARROW_READING}, Milliseconds(0)),
            look(/*events=*/{}, {TUTORIAL_CARD_READING}, Milliseconds(500)),
            look({ScreenEvent::BATTLE_STARTED}, /*readings=*/{}, Milliseconds(1000)),
        }},
        .expectation = {.verdict = "done", .line = "[PLZA story_battle_start] done on battle."},
    },
    {
        //  console B, 2026-11-16 (run 3): the scene before Taunie handed
        //  control back with no battle, and the battle start waited 90 s.
        .name = "a battle start that ends in control, the party bar held 5 s, fails at once",
        //  The fade's wait takes the first look after it: the party bar's
        //  5 s run from the look after that.
        .input = {.checkpoint = "03.02b", .first_step = 4, .steps = 1, .looks = {
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(0)),
            party_bar(Milliseconds(1000)), party_bar(Milliseconds(1000)), party_bar(Milliseconds(3000)), party_bar(Milliseconds(6100)),
        }},
        .expectation = {.verdict = "failed at taunie_battle_start: scene_ended_in_control"},
    },
    {
        .name = "a battle start goes on through the party bar between the talk's lines",
        .input = {.checkpoint = "03.02b", .first_step = 4, .steps = 1, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            party_bar(Milliseconds(1000)), party_bar(Milliseconds(4000)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(5000)),
            party_bar(Milliseconds(6000)),
            look({ScreenEvent::BATTLE_STARTED}, /*readings=*/{}, Milliseconds(9000)),
        }},
        .expectation = {.verdict = "done", .line = "[PLZA story_battle_start] done on battle."},
    },
    {
        .name = "03.02b's map talk: A on the text, B on the map, done on the party bar held a second",
        .input = {.checkpoint = "03.02b", .first_step = 8, .steps = 1, .looks = {
            DIALOG, look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)), party_bar(Milliseconds(4000)), party_bar(Milliseconds(5100)),
        }},
        .expectation = {
            .verdict = "done",
            .inputs = "press A, press B, wait 250ms",
            .line = "[PLZA] talk_through_map_screen: the party bar held: back in the overworld",
        },
    },
    {
        .name = "03.02b's map talk goes on through the party bar under a text box",
        .input = {.checkpoint = "03.02b", .first_step = 8, .steps = 1, .looks = {
            look({ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)),
            look({ScreenEvent::PLAYER_CAN_MOVE, ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(2000)),
            party_bar(Milliseconds(4000)), party_bar(Milliseconds(5100)),
        }},
        .expectation = {.verdict = "done", .inputs = "press A, press A, wait 250ms"},
    },
    {
        //  console B, 2026-11-16 (03.02b run 5): the "Map & Missions" card
        //  (2 pages) over the map app. A turns its pages and closes it on
        //  the last; B turns back, so A and B in turn held it on page 1
        //  for 120 s. Then B closes the map and goes through her talk.
        .name = "03.02b's map talk pages the Map & Missions card with A only, then B on the map and her talk",
        .input = {.checkpoint = "03.02b", .first_step = 8, .steps = 1, .looks = {
            look(/*events=*/{}, {TUTORIAL_CARD_READING}, Milliseconds(0)),
            look(/*events=*/{}, {TUTORIAL_CARD_READING}, Milliseconds(1000)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2000)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(3000)),
            party_bar(Milliseconds(4000)), party_bar(Milliseconds(5100)),
        }},
        .expectation = {.verdict = "done", .inputs = "press A, press A, press B, press B, wait 250ms"},
    },
    {
        .name = "03.02b's map talk fails with no party bar in 120 s",
        .input = {.checkpoint = "03.02b", .first_step = 8, .steps = 1, .looks = {
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(0)), look(/*events=*/{}, /*readings=*/{}, Milliseconds(121000)),
        }},
        .expectation = {.verdict = "failed at after_taunie_battle: map_talk_timeout", .inputs = "press B"},
    },
    {
        .name = "03.03 climbs the ladder: sidestep right, face it, A, then up to the top",
        .input = {.checkpoint = "03.03", .first_step = 3, .steps = 1, .looks = {CUTSCENE}},
        .expectation = {
            .verdict = "done",
            .inputs = "stick 90deg 600ms walk, release 300ms, stick 0deg 150ms walk, release 300ms, press A, "
                "stick 0deg 5000ms walk, release 500ms",
        },
    },
    {
        .name = "03.04's Pokedex: B at the app and at its text, done on the party bar held a second",
        .input = {.checkpoint = "03.04", .first_step = 2, .steps = 1, .looks = {
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(0)), look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(500)),
            party_bar(Milliseconds(1000)), party_bar(Milliseconds(2100)),
        }},
        .expectation = {.verdict = "done", .inputs = "press B, press B, wait 250ms"},
    },
    {
        .name = "03.07's run to the gate needs a console",
        .input = {.checkpoint = "03.07", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at to_zone_1_gate: console_only", .inputs = ""},
    },
    {
        .name = "03.07's tutorial catch needs a console",
        .input = {.checkpoint = "03.07", .first_step = 2, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at catch_fletchling: console_only", .inputs = ""},
    },
    {
        .name = "03.08's patrol for two more species needs a console",
        .input = {.checkpoint = "03.08", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at catch_more_species: console_only", .inputs = ""},
    },
    {
        .name = "03.08b's walk back to Wild Zone 1's road needs a console",
        .input = {.checkpoint = "03.08b", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at zone_1_back_to_road: console_only", .inputs = ""},
    },
    {
        .name = "03.08b's run to the far gate needs a console",
        .input = {.checkpoint = "03.08b", .first_step = 1, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at zone_1_to_far_gate: console_only", .inputs = ""},
    },
    {
        //  The still screen's wait looks until the screen changes: it
        //  takes the first party bar, so the machine's own looks start at
        //  the next one (as in 03.02b's plaza scene).
        .name = "03.08b's way out of the gate: A Leave at its prompt, through the fade, done on the party bar held a second",
        .input = {.checkpoint = "03.08b", .first_step = 2, .steps = 1, .looks = {
            A_PROMPT, BLACK_FADE,
            look({ScreenEvent::SCREEN_STILL}, /*readings=*/{}, Milliseconds(1000)),
            party_bar(Milliseconds(2000)), party_bar(Milliseconds(2250)), party_bar(Milliseconds(3350)),
        }},
        .expectation = {.verdict = "done", .line = "[PLZA mash_to_move_hints] done on can_move."},
    },
    {
        .name = "03.09's walk to the objective marker needs a console",
        .input = {.checkpoint = "03.09", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at to_research_lab: console_only", .inputs = ""},
    },
    {
        .name = "03.11's reward needs a console",
        .input = {.checkpoint = "03.11", .first_step = 2, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at rock_smash_tm: console_only", .inputs = ""},
    },
};

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console(builtin_story_data(), sightings);
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
    Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console(rows, sightings);
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
            .line = "[Checkpoint] 01.02: off the station platform; START UNCHECKED: nothing reads this row's own start",
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
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = test.input.has_reload
        ? game_with_reload(story, sightings)
        : make_plza_story_game_without_console(story, sightings);
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
//  its platform wait then needs two platform looks without the badge.
struct ChainCase{
    std::string name;
    struct Input{
        std::set<std::string, std::less<>> reload_always;
        std::vector<Observation<ZonePose>> looks;
        //  The actions of 01.01's steps; a repeated one's step is named
        //  "<action>_again".
        std::vector<std::string> first_actions = {"wait_for_platform"};
        std::string second_action = "text_to_choice";
        size_t attempts = 1;
        SegmentRunOptions::ForcedReloads forced_reloads = SegmentRunOptions::ForcedReloads::KEEP;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  How many times the reload pressed HOME.
        size_t reloads = 0;
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const Observation<ZonePose> PLATFORM_SAVING = look({}, {PLATFORM_READING, AUTOSAVE_READING});

const ChainCase CHAIN_CASES[] = {
    {
        .name = "autosave seen and the start check does not fail: on without a reload",
        .input = {.reload_always = {}, .looks = {PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "no autosave seen and the start check does not fail: on without a reload",
        .input = {.reload_always = {}, .looks = {PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "autosave seen and the start check fails: reloads the start",
        .input = {.reload_always = {}, .looks = {
            PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0),
        }},
        .expectation = {.verdict = "done", .reloads = 1},
    },
    {
        .name = "no autosave seen and the start check fails: no reload, no save to start from",
        .input = {.reload_always = {}, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at 01.02 chain: no_start_save", .reloads = 0},
    },
    {
        .name = "reload always, autosave seen: reloads the start",
        .input = {.reload_always = {"01.02"}, .looks = {PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 1},
    },
    {
        .name = "reload always, no autosave seen: no reload, no save to start from",
        .input = {.reload_always = {"01.02"}, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at 01.02 chain: no_start_save", .reloads = 0},
    },
    {
        .name = "reload always, forced reloads skipped: on from the live screen",
        .input = {
            .reload_always = {"01.02"},
            .looks = {PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)},
            .forced_reloads = SegmentRunOptions::ForcedReloads::SKIP,
        },
        .expectation = {.verdict = "done", .reloads = 0, .line = "[Checkpoint] 01.02: reload skipped (developer option)"},
    },
    {
        .name = "reload always, forced reloads skipped, no autosave seen: on from the live screen",
        .input = {
            .reload_always = {"01.02"},
            .looks = {PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)},
            .forced_reloads = SegmentRunOptions::ForcedReloads::SKIP,
        },
        .expectation = {.verdict = "done", .reloads = 0, .line = "[Checkpoint] 01.02: reload skipped (developer option)"},
    },
    {
        .name = "a reload onto an older save stops",
        .input = {.reload_always = {"01.02"}, .looks = {PLATFORM_SAVING, PLATFORM}},
        .expectation = {.verdict = "failed at 01.02 chain: older_save", .reloads = 1},
    },
    {
        //  console B, 2026-11-16: 03.06 autosaved in wild_zones_tutorial and
        //  walked on to Wild Zone 1's gate; 03.07 reloaded that save, outside
        //  the gate, and threw Poke Balls there.
        .name = "an autosave before the last step does not save the row: no save to reload the next from",
        .input = {
            .reload_always = {"01.02"},
            .looks = {PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM},
            .first_actions = {"wait_for_platform", "wait_for_platform"},
        },
        .expectation = {
            .verdict = "failed at 01.02 chain: no_start_save", .reloads = 0,
            .line = "[Save] 01.01: the game autosaved in wait_for_platform, before wait_for_platform_again: not saved, a reload would skip wait_for_platform_again",
        },
    },
    {
        //  console B, 2026-11-16: 03.04 autosaved in pokedex_app and failed
        //  in the step after it; the next run's 03.04 loaded that save,
        //  outside the Center, and its walk to the Center found no prompt.
        .name = "a run that autosaved and then failed does not reload: the save moved past its start",
        .input = {
            .reload_always = {},
            .looks = {PLATFORM_SAVING, PLATFORM, PLATFORM, look(/*events=*/{}, {AUTOSAVE_READING}, Milliseconds(10000)), CUTSCENE},
            .second_action = "wait_for_platform",
            .attempts = 2,
        },
        .expectation = {
            .verdict = "failed at 01.02 : reload_failed", .reloads = 0,
            .line = "[Save] 01.02: the game autosaved in wait_for_platform before the run failed: the latest save is past 01.02's start",
        },
    },
};

UnitTestResult check_chain(const ChainCase& test, CancellableScope& scope){
    Pokemon::StoryRows story;
    story.reload_always = test.input.reload_always;
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = game_with_reload(story, sightings);
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    StoryCheckpoint first = one_step_row("01.01", test.input.first_actions[0]);
    for (size_t index = 1; index < test.input.first_actions.size(); index++){
        const std::string& action = test.input.first_actions[index];
        first.steps.push_back(StoryStep{.kind = Pokemon::StoryStepKind::GAME, .name = std::format("{}_again", action), .why = "", .action = action, .args = {}});
    }
    const std::vector<StoryCheckpoint> rows{first, one_step_row("01.02", test.input.second_action)};
    const Pokemon::SegmentReport report = run_plza_segments(
        game, story, rows, {.attempts_per_checkpoint = test.input.attempts, .forced_reloads = test.input.forced_reloads},
        backend, state, sightings
    );
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
        .input = {.action = "walk_to_marker"},
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
        //  console B, 2026-11-16 22:58: 02.04's check-in autosave was still
        //  up 1.2 s later on 02.05's first look, and 02.05's failure then
        //  took its start save for gone and rewound to the backup save.
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
        //  console B, 2026-11-16 23:54: 02.07 ended on the mission banner
        //  with no badge seen; 02.07's autosave on entering Room 202 was
        //  still up on 03.01's first look, 37 ms in, and 03.01's failure
        //  then took its start save for gone.
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
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
