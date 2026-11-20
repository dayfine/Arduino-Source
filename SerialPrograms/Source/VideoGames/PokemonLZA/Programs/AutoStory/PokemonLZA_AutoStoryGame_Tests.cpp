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
#include <span>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h"
#include "PokemonLZA_AutoStoryMenuReadings.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryGame_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;


std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string text;
    for (const std::string& part : texts){
        text += (text.empty() ? "" : std::string(separator)) + part;
    }
    return text;
}


struct AutosaveStepCase{
    std::string name;
    struct Input{
        std::vector<std::string> steps;
        bool marked_autosave = false;
        std::optional<std::string> autosave_step = std::nullopt;
    };
    Input input;
    struct Expectation{
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
            .marked_autosave = true,
            .autosave_step = "wild_zones_tutorial",
        },
        .expectation = {.error = ""},
    },
};

UnitTestResult check_autosave_step(const AutosaveStepCase& test, CancellableScope&){
    StoryCheckpoint checkpoint;
    checkpoint.id = "03.06";
    checkpoint.title = "Test";
    for (const std::string& name : test.input.steps){
        checkpoint.steps.push_back(StoryStep{.name = name});
    }
    const std::optional<std::string> error = autosave_step_error(checkpoint, test.input.marked_autosave, test.input.autosave_step);
    const std::string got = error.value_or("");
    if (got != test.expectation.error){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.error, got);
    }
    return true;
}


//  `at`: after the scripts' common start.
ScreenObservation look(ScreenEvents events, std::vector<std::string_view> readings = {}, Milliseconds at = Milliseconds(0)){
    ScreenObservation observation;
    observation.when = WallClock::min() + at;
    observation.events = events;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

//  01.01's platform: the controls hint, no party bar.
const ScreenObservation PLATFORM = look({}, {CONTROLS_HINT_READING});
//  2 s in: a badge in a checkpoint's first 1.5 s is the save before it.
const ScreenObservation OWN_AUTOSAVE = look({}, {AUTOSAVE_READING}, Milliseconds(2000));
const ScreenObservation CUTSCENE = look({});
const ScreenObservation STILL = look({ScreenEvent::SCREEN_STILL});
const ScreenObservation WHITE_FADE = look({ScreenEvent::FADE_WHITE});
const ScreenObservation BLACK_FADE = look({ScreenEvent::FADE_BLACK});
const ScreenObservation DIALOG = look({ScreenEvent::DIALOG_BOX});
const ScreenObservation A_PROMPT = look({ScreenEvent::INTERACT_PROMPT, ScreenEvent::PLAYER_CAN_MOVE});
//  01.03 shows no party bar.
const ScreenObservation PROMPT_ONLY = look({ScreenEvent::INTERACT_PROMPT});
//  A text page typed out: the text box and its advance arrow.
const ScreenObservation TEXT_PAGE = look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING});

ScreenObservation party_bar(Milliseconds at){
    return look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, at);
}

ScreenObservation text_page(Milliseconds at){
    return look({ScreenEvent::DIALOG_BOX}, {ADVANCE_ARROW_READING}, at);
}

ScreenObservation choice(int selected, int rows = 2, Milliseconds at = Milliseconds(0)){
    ScreenObservation observation = look({ScreenEvent::DIALOG_BOX, ScreenEvent::CHOICE_BOX}, /*readings=*/{}, at);
    observation.readings[std::string(CHOICE_ROWS_READING)] = rows;
    observation.readings[std::string(CHOICE_SELECTED_READING)] = selected;
    return observation;
}

//  `count` party bar looks `every` apart from the start.
std::vector<ScreenObservation> party_bars(Milliseconds every, size_t count){
    std::vector<ScreenObservation> looks;
    for (size_t index = 0; index < count; index++){
        looks.push_back(party_bar(every * (int64_t)index));
    }
    return looks;
}

std::vector<ScreenObservation> concat(std::vector<ScreenObservation> first, const std::vector<ScreenObservation>& then){
    first.insert(first.end(), then.begin(), then.end());
    return first;
}

std::string repeated(std::string_view text, size_t times){
    return joined(std::vector<std::string>(times, std::string(text)), ", ");
}


//  Steps [first_step, first_step + steps) of built-in checkpoint `id`,
//  bound to `tools`.
StoryCheckpoint builtin_row(PlzaConsoleHooks& tools, std::string_view id, size_t first_step, size_t steps){
    for (StoryCheckpoint& row : load_plza_story(tools).checkpoints){
        if (row.id == id){
            row.steps.erase(row.steps.begin(), row.steps.begin() + (std::ptrdiff_t)first_step);
            if (steps < row.steps.size()){
                row.steps.resize(steps);
            }
            return std::move(row);
        }
    }
    return {};
}

//  "done", or "failed at [<checkpoint> ]<step, or chain>: <code>".
std::string verdict_of(const std::optional<Pokemon::CheckpointFailure>& failure, std::span<const StoryCheckpoint> rows, bool with_checkpoint){
    if (!failure){
        return "done";
    }
    std::string event = "chain";
    for (const StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == failure->checkpoint_id && failure->event_index){
            event = checkpoint.steps[*failure->event_index].name;
        }
    }
    if (!with_checkpoint){
        return std::format("failed at {}: {}", event, failure->failure.code);
    }
    return std::format("failed at {} {}: {}", failure->checkpoint_id, event, failure->failure.code);
}


struct RunCase{
    std::string name;
    struct Input{
        std::string checkpoint;
        size_t first_step = 0;
        size_t steps = std::numeric_limits<size_t>::max();
        std::vector<ScreenObservation> looks;
        //  The last repeats; none: no minimap arrow, as off a console.
        std::vector<double> arrow_reads = {};
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <event>: <failure>".
        std::string verdict;
        //  None: not checked.
        std::optional<std::string> inputs = std::nullopt;
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
        .name = "01.00b's opening fails with no autosave",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {PLATFORM}},
        .expectation = {.verdict = "failed at opening: platform_not_seen"},
    },
    {
        .name = "01.01's walk plays a dropped leg again",
        .input = {.checkpoint = "01.01", .first_step = 2, .steps = 1, .looks = {PLATFORM, PLATFORM, PLATFORM, STILL, PLATFORM, WHITE_FADE}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, stick 0deg 4500ms run, stick 0deg 4500ms run, press L, stick 90deg 150ms walk, release 100ms, press L, stick 0deg 250ms run",
            .line = "[PLZA] walk: the screen stayed still after the walk of leg 1/2 turn 0 walk 4500 ms; once more",
        },
    },
    {
        //  Control comes ~7 s after 01.01's scenes end (console B, 2026-11-15).
        .name = "a walk waits for control that comes 7 s after the scene",
        .input = {.checkpoint = "01.01", .first_step = 2, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(28, CUTSCENE),
            {PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, WHITE_FADE}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = repeated("wait 250ms", 29) +
                ", stick 0deg 4500ms run, press L, stick 90deg 150ms walk, release 100ms, press L, stick 0deg 250ms run",
        },
    },
    {
        .name = "01.01's walk does not start in a text box",
        .input = {.checkpoint = "01.01", .first_step = 2, .looks = {DIALOG}},
        .expectation = {.verdict = "failed at platform_to_plaza: walk_not_free"},
    },
    {
        //  console B, 2026-11-19: a Plus 4 ms into the step, before the opening faded in, did nothing.
        .name = "01.00b holds Plus 0.75 s after the opening fades in, not during the fade, and ends on its autosave",
        .input = {.checkpoint = "01.00b", .first_step = 1, .looks = {BLACK_FADE, CUTSCENE, look(/*events=*/{}, /*readings=*/{}, Milliseconds(750)), OWN_AUTOSAVE, PLATFORM, PLATFORM}},
        .expectation = {.verdict = "done", .inputs = "wait 300ms, wait 300ms, press +, wait 300ms, wait 300ms, wait 300ms", .line = "[Save] 01.00b: the game autosaved (seen)"},
    },
    {
        .name = "01.02's choice fails when the highlight does not move",
        .input = {.checkpoint = "01.02", .first_step = 2, .steps = 1, .looks = {choice(0)}},
        .expectation = {.verdict = "failed at battle_with_pokemon: choice_not_reached"},
    },
    {
        .name = "02.06's talk fails when nothing starts after A: the prompt stays",
        .input = {.checkpoint = "02.06", .first_step = 1, .steps = 1, .looks = {A_PROMPT}},
        .expectation = {.verdict = "failed at rest: talk_no_scene"},
    },
    {
        .name = "a talk presses A again while the A prompt stays with nothing open",
        .input = {.checkpoint = "02.06", .first_step = 1, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(7, A_PROMPT), {DIALOG}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = std::format("wait 250ms, press A, {}, press A, wait 250ms", repeated("wait 250ms", 4)),
            .line = "[PLZA] talk: the A prompt still up 1000 ms after A 1 with nothing open; A again",
        },
    },
    {
        .name = "a talk presses nothing on an A prompt seen on one look only",
        .input = {.checkpoint = "02.06", .first_step = 1, .steps = 1, .looks = {A_PROMPT, CUTSCENE}},
        .expectation = {.verdict = "failed at rest: talk_no_prompt", .inputs = repeated("wait 250ms", 12)},
    },
    {
        .name = "a story battle with no battle HUD in 10 s fails as battle_never_started, the battle code not run",
        .input = {.checkpoint = "01.03", .first_step = 2, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at andi_battle: battle_never_started", .inputs = repeated("wait 250ms", 40)},
    },
    {
        //  Held legs run with B held: the walk times stay walk times (console B, 2026-11-19).
        .name = "02.01 waits 3 s for the NPCs, then runs its held legs",
        .input = {.checkpoint = "02.01", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 3000ms, wait 250ms, stick 10deg 4250ms run, press L, stick 90deg 6000ms run, press L, stick 90deg 6500ms run",
        },
    },
    {
        .name = "02.02's street scene fails when the party bar shows 45 s and it never starts",
        .input = {.checkpoint = "02.02", .first_step = 1, .steps = 1, .looks = {party_bar(Milliseconds(0)), party_bar(Milliseconds(46000))}},
        .expectation = {.verdict = "failed at street_scene: scene_never_started", .inputs = "wait 250ms"},
    },
    {
        //  From the save the street scene ends at the gate, its prompt up.
        .name = "02.02's walk to the gate talks at once on the A prompt it starts at",
        .input = {.checkpoint = "02.02", .first_step = 2, .steps = 1, .looks = concat(std::vector<ScreenObservation>(4, A_PROMPT), {DIALOG})},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, wait 300ms, press A, wait 250ms",
            .line = "[PLZA] walk: the A prompt shows before the walk: no walk.",
        },
    },
    {
        .name = "02.05's walk to room 202 starts after 10 s with nothing busy: the hall shows no party bar, then the door's A prompt",
        .input = {.checkpoint = "02.05", .first_step = 5, .steps = 1, .looks = concat(std::vector<ScreenObservation>(50, CUTSCENE), {A_PROMPT})},
        .expectation = {
            .verdict = "done",
            .inputs = std::format("{}, press L, stick -15deg 150ms walk, release 100ms, press L, {}, wait 300ms", repeated("wait 250ms", 40), repeated("stick 0deg 250ms run", 8)),
            .line = "[PLZA] walk: nothing busy for 10000 ms and no sign of control; taken as control.",
        },
    },
    {
        .name = "02.06 with no autosave badge seen is not saved",
        .input = {.checkpoint = "02.06", .first_step = 4, .looks = {
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(0)), party_bar(Milliseconds(21000)), party_bar(Milliseconds(22000)),
        }},
        .expectation = {.verdict = "done", .line = "[Save] 02.06: not saved (the plan's autosave was not seen)"},
    },
    {
        .name = "a walk to an A prompt holds on past a glimpse gone again 300 ms later",
        .input = {.checkpoint = "03.02b", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(5, party_bar(Milliseconds(0))), {A_PROMPT, party_bar(Milliseconds(0)), A_PROMPT}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, stick 0deg 250ms run, stick 0deg 250ms run, wait 300ms, stick 0deg 250ms run, wait 300ms",
            .line = "[PLZA] walk: the end event after 750 ms of the last leg.",
        },
    },
    {
        //  Alton's prompt shows beside the path 500-750 ms in; the door's at 2000 ms.
        .name = "02.05's walk to the elevator walks past a prompt before its measured time to the one at the door",
        .input = {.checkpoint = "02.05", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(4, party_bar(Milliseconds(0))),
            concat({A_PROMPT, A_PROMPT}, concat(std::vector<ScreenObservation>(4, party_bar(Milliseconds(0))), {A_PROMPT}))
        )},
        .expectation = {
            .verdict = "done",
            .inputs = std::format("wait 250ms, {}, wait 300ms", repeated("stick 0deg 250ms run", 8)),
            .line = "[PLZA] walk: the end event after 2000 ms of the last leg.",
        },
    },
    {
        .name = "03.02b's walk to Taunie: A on her A prompt, done when her subtitle starts (the advance chevron with no text box)",
        .input = {.checkpoint = "03.02b", .first_step = 2, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(4, party_bar(Milliseconds(0))), {A_PROMPT, look({ScreenEvent::SCREEN_STILL}, {ADVANCE_ARROW_READING})}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, stick 0deg 250ms run, press A, wait 250ms",
            .line = "[PLZA] walk: A on the A prompt after 250 ms of the last leg started a talk.",
        },
    },
    {
        //  02.05 on console B 2026-11-19 17:37: A on "nothing" after the fade
        //  and a second A on the same text page 90 ms after the first; one
        //  landed on the floor choice's top row and the elevator closed.
        .name = "02.05's floor question: nothing pressed on a screen with no text, one A per text page, done at the choice",
        .input = {.checkpoint = "02.05", .first_step = 2, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(4, BLACK_FADE),
            {CUTSCENE, CUTSCENE, TEXT_PAGE, TEXT_PAGE, TEXT_PAGE, choice(0)}
        )},
        .expectation = {
            .verdict = "done",
            .inputs = std::format("{}, press A, wait 250ms, wait 250ms", repeated("wait 250ms", 6)),
            .line = "[PLZA text_to_choice] done on choice.",
        },
    },
    {
        //  03.05 on console B 2026-11-19 18:58: after the heal the nurse asks "anything else?" with
        //  her menu again; A took its top row, the heal, and the talk looped 120 s.
        .name = "03.05's heal: A on the nurse's menu, B when it comes back after her text, done at control",
        .input = {.checkpoint = "03.05", .first_step = 1, .steps = 1, .looks = {
            text_page(Milliseconds(0)), choice(/*selected=*/0, /*rows=*/3, Milliseconds(400)), text_page(Milliseconds(800)),
            text_page(Milliseconds(1200)), choice(/*selected=*/0, /*rows=*/3, Milliseconds(1600)), text_page(Milliseconds(2000)),
            party_bar(Milliseconds(2400)), party_bar(Milliseconds(3500)),
        }},
        .expectation = {.verdict = "done", .inputs = "press A, press A, press A, press A, press B, press A, wait 250ms",
            .line = "[PLZA scene] choice: 3 rows, shown again after A took it: B"},
    },
    {
        //  03.11 on console B 2026-11-19 19:27: B on Mable's second 2-row question did nothing, for 120 s.
        .name = "03.11's Mable talk: a second choice like the first gets B, still up after it gets A, done at control",
        .input = {.checkpoint = "03.11", .first_step = 1, .steps = 1, .looks = {
            text_page(Milliseconds(0)), choice(/*selected=*/0, /*rows=*/2, Milliseconds(400)), text_page(Milliseconds(800)),
            text_page(Milliseconds(1200)), choice(/*selected=*/0, /*rows=*/2, Milliseconds(1600)),
            choice(/*selected=*/0, /*rows=*/2, Milliseconds(2000)), text_page(Milliseconds(2400)),
            party_bar(Milliseconds(2800)), party_bar(Milliseconds(3900)),
        }},
        .expectation = {.verdict = "done", .inputs = "press A, press A, press A, press A, press B, press A, press A, wait 250ms",
            .line = "[PLZA scene] done on control."},
    },
    {
        //  01.03 on console B 2026-11-19 17:10: Taunie's A prompt shows at the
        //  start and on the way; an A on it mid-walk took her line, not the
        //  starter's. The starter walk presses A only once its legs are done.
        //  The turn looks twice, then each 250 ms slice of the legs once.
        .name = "01.03's walk to the starter: no A on a prompt during its legs, A on the prompt after them, done when the talk starts",
        .input = {.checkpoint = "01.03", .first_step = 0, .steps = 1, .looks = concat(
            std::vector<ScreenObservation>(5, PROMPT_ONLY),
            concat(std::vector<ScreenObservation>(8, A_PROMPT), {A_PROMPT, DIALOG})
        )},
        .expectation = {
            .verdict = "done",
            .inputs = std::format(
                "{}, press L, stick -70deg 150ms walk, release 100ms, press L, {}, press A, wait 250ms",
                repeated("wait 250ms", 4), repeated("stick 0deg 250ms run", 6)
            ),
            .line = "[PLZA] walk: A on the A prompt after 1500 ms of the last leg started a talk.",
        },
    },
    {
        .name = "a battle start that ends in control, the party bar held 5 s and no lock-on watch, fails",
        //  The fade's wait takes the first look after it.
        .input = {.checkpoint = "03.02b", .first_step = 3, .steps = 1, .looks = {
            look({ScreenEvent::FADE_BLACK}, /*readings=*/{}, Milliseconds(0)),
            party_bar(Milliseconds(1000)), party_bar(Milliseconds(1000)), party_bar(Milliseconds(3000)), party_bar(Milliseconds(6100)),
        }},
        .expectation = {.verdict = "failed at taunie_battle_start: scene_ended_in_control"},
    },
    {
        .name = "03.02b's map talk fails with no party bar in 120 s",
        .input = {.checkpoint = "03.02b", .first_step = 7, .steps = 1, .looks = {
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(0)), look(/*events=*/{}, /*readings=*/{}, Milliseconds(121000)),
        }},
        .expectation = {.verdict = "failed at after_taunie_battle: map_talk_timeout", .inputs = "press B"},
    },
    {
        .name = "03.03's walk off the roof fails when no scene takes control",
        .input = {.checkpoint = "03.03c", .first_step = 0, .steps = 1, .looks = {party_bar(Milliseconds(0))}},
        .expectation = {.verdict = "failed at off_the_roof: walk_no_scene"},
    },
    {
        //  2026-11-19 03.03, ES-LA: 250 ms run slices took 111 ms; the leg ended 3.6 s in.
        .name = "03.03's walk off the roof holds its last leg 8000 ms of game time, though a run slice takes less",
        .input = {.checkpoint = "03.03c", .first_step = 0, .steps = 1, .looks = concat(
            party_bars(Milliseconds(111), 60),
            {look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(6700)), look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(6800))}
        )},
        .expectation = {.verdict = "done"},
    },
    //  Arrow reads from a passing run: 272 out of the gate, 254 after one turn.
    {
        .name = "03.09's walk faces the street first, then walks its legs into Ryon's dialog",
        .input = {
            .checkpoint = "03.09", .first_step = 0, .steps = 1,
            .looks = concat(std::vector<ScreenObservation>(7, party_bar(Milliseconds(0))), {DIALOG}),
            .arrow_reads = {272, 254},
        },
        .expectation = {
            .verdict = "done",
            .inputs = "wait 250ms, press L, stick -15deg 150ms walk, release 100ms, press L, stick 0deg 6500ms run, "
                "press L, stick -90deg 150ms walk, release 100ms, press L, stick 0deg 250ms run",
            .line = "[PLZA] walk: the end event after 250 ms of the last leg.",
        },
    },
};

//  The last read repeats; none for no reads.
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
    PlzaConsoleHooks tools{.arrow = scripted_arrow(test.input.arrow_reads), .sightings = &sightings};
    tools.battle = [](PlzaBackend&) -> std::optional<Pokemon::StoryFailure>{
        return std::nullopt;
    };
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    std::vector<StoryCheckpoint> rows;
    rows.push_back(builtin_row(tools, test.input.checkpoint, test.input.first_step, test.input.steps));
    note_autosaves(rows, sightings);
    const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(backend, rows, game, state);
    const std::string verdict = verdict_of(failure, rows, /*with_checkpoint=*/false);
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


//  The reload presses HOME, as the console's does first, and says the game
//  is at the failed checkpoint's start.
Pokemon::StoryGame game_with_reload(const MarkedCheckpoints& rows, AutosaveSightings& sightings){
    Pokemon::StoryGame game = plza_story_game(rows, sightings);
    game.reload = [](PlzaBackend& backend, const StoryCheckpoint& failed) -> std::optional<std::string>{
        backend.press(BUTTON_HOME, Milliseconds(80), Milliseconds(80));
        return failed.id;
    };
    return game;
}

//  A step named `name` running `action` with no args, bound to `tools`.
StoryStep plan_step(PlzaConsoleHooks& tools, std::string name, std::string_view action){
    return StoryStep{.name = std::move(name), .action = std::string(action), .run = plza_story_actions(tools).at(std::string(action))(JsonObject())};
}

StoryCheckpoint one_step_row(PlzaConsoleHooks& tools, std::string id, std::string action){
    StoryCheckpoint row;
    row.id = std::move(id);
    row.title = "Test";
    row.steps.push_back(plan_step(tools, action, action));
    return row;
}


struct LoadCase{
    std::string name;
    struct Input{
        std::string checkpoint;     //  A built-in one.
        bool has_reload = true;
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string verdict;
        std::string inputs;
        std::string line = {};
    };
    Expectation expectation;
};

const LoadCase LOAD_CASES[] = {
    {
        .name = "01.02's start: loaded, and not checked",
        .input = {.checkpoint = "01.02", .looks = {PLATFORM}},
        .expectation = {
            .verdict = "done", .inputs = "press HOME",
            .line = "[Checkpoint] 01.02: START UNCHECKED: nothing reads this row's own start",
        },
    },
    {
        .name = "01.01's start is left to its first step's platform check",
        .input = {.checkpoint = "01.01", .looks = {CUTSCENE}},
        .expectation = {.verdict = "done", .inputs = "press HOME"},
    },
};

UnitTestResult check_load(const LoadCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::StoryGame game = test.input.has_reload ? game_with_reload(builtin_story_data(), sightings) : plza_story_game(builtin_story_data(), sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    PlzaConsoleHooks tools;
    const std::optional<Pokemon::StoryFailure> loaded = load_checkpoint_start(game, backend, builtin_row(tools, test.input.checkpoint, /*first_step=*/0, /*steps=*/99));
    const std::string verdict = !loaded ? "done" : loaded->code;
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
//  After the badge, the platform wait needs two platform looks without it.
struct ChainCase{
    std::string name;
    struct Input{
        std::vector<ScreenObservation> looks;
        //  A repeated action's step is named "<action>_again".
        std::vector<std::string> first_actions = {"wait_for_platform"};
        std::string second_action = "text_to_choice";
        size_t attempts = 1;
        bool reload_when_chained = false;
        StartSignature landed = {};
        std::optional<StartSignature> start = std::nullopt;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        size_t reloads = 0;
        std::string line = {};
        bool nothing_pressed_after_reload = false;
    };
    Expectation expectation;
};

const ScreenObservation PLATFORM_SAVING = look({}, {CONTROLS_HINT_READING, AUTOSAVE_READING}, Milliseconds(2000));
const ScreenObservation IN_CONTROL = look({ScreenEvent::PLAYER_CAN_MOVE});
const std::vector<ScreenObservation> ROOF_SAVED = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM};
//  01.02's scene after the reload.
const std::vector<ScreenObservation> AFTER_RELOAD = {
    DIALOG,
    look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, Milliseconds(1000)), look({ScreenEvent::PLAYER_CAN_MOVE}, /*readings=*/{}, Milliseconds(3000)),
};
const StartSignature STATION_FRONT{.talk_prompt = false, .party = 1};

const ChainCase CHAIN_CASES[] = {
    {
        .name = "autosave seen: on from the live screen without a reload",
        .input = {.looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, CUTSCENE, CUTSCENE, CUTSCENE, choice(0)}},
        .expectation = {.verdict = "done", .reloads = 0},
    },
    {
        .name = "an autosave before the last step does not save the row",
        .input = {
            .looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, PLATFORM, PLATFORM},
            .first_actions = {"wait_for_platform", "wait_for_platform"},
            .second_action = "wait_for_platform",
        },
        .expectation = {
            .verdict = "done", .reloads = 0,
            .line = "[Save] 01.01: not saved (the game autosaved in wait_for_platform, before wait_for_platform_again: a reload would skip wait_for_platform_again)",
        },
    },
    {
        .name = "a run that autosaved and then failed does not reload: the save moved past its start",
        .input = {
            .looks = {CUTSCENE, PLATFORM_SAVING, PLATFORM, PLATFORM, CUTSCENE, look(/*events=*/{}, {AUTOSAVE_READING}, Milliseconds(10000)), CUTSCENE},
            .second_action = "wait_for_platform",
            .attempts = 2,
        },
        .expectation = {
            .verdict = "failed at 01.02 chain: start_unknown", .reloads = 0,
            .line = "[Save] 01.02: the game autosaved in wait_for_platform before the run failed: the latest save is past 01.02's start",
        },
    },
    //  As 03.03 into 03.04: the badge shows in the row's first step, not its last.
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
    Pokemon::StoryGame game = game_with_reload(MarkedCheckpoints{}, sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    PlzaConsoleHooks tools{.sightings = &sightings};
    std::vector<StoryCheckpoint> rows;
    rows.push_back(one_step_row(tools, "01.01", test.input.first_actions[0]));
    for (size_t index = 1; index < test.input.first_actions.size(); index++){
        const std::string& action = test.input.first_actions[index];
        rows[0].steps.push_back(plan_step(tools, std::format("{}_again", action), action));
    }
    rows.push_back(one_step_row(tools, "01.02", test.input.second_action));
    note_autosaves(rows, sightings);
    std::vector<CheckpointStart> starts;
    if (test.input.start){
        starts.push_back(CheckpointStart{.checkpoint = "01.02", .signature = *test.input.start});
    }
    CheckpointRunOptions options{.attempts_per_checkpoint = test.input.attempts, .starts = starts};
    options.read_start = [&test](const StartSignature&){
        return test.input.landed;
    };
    if (test.input.reload_when_chained){
        options.reload_when_chained = {"01.02"};
    }
    const std::optional<Pokemon::CheckpointFailure> failure = run_plza_checkpoints(game, rows, options, backend, state, sightings);
    const std::string verdict = verdict_of(failure, rows, /*with_checkpoint=*/true);
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


//  The new game rows hand over to the platform with no save loaded between them.
struct NewGameChainCase{
    std::string name;
    struct Input{
        std::vector<std::string> rows;
        std::vector<ScreenObservation> looks;
    };
    Input input;
    struct Expectation{
        std::string verdict;
        std::string console_steps;
    };
    Expectation expectation;
};

const NewGameChainCase NEW_GAME_CHAIN_CASES[] = {
    {
        .name = "01.00a, 01.00b and 01.01 in one run",
        //  01.01: the platform for the start check, the text speed's menu, the walk's free control.
        .input = {.rows = {"01.00a", "01.00b"}, .looks = {
            CUTSCENE, OWN_AUTOSAVE, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, look({}, {MAIN_MENU_READING}), look({}, {MAIN_MENU_READING}),
            CUTSCENE, CUTSCENE, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, PLATFORM, WHITE_FADE,
        }},
        .expectation = {.verdict = "done", .console_steps = "new_game_intro confirm_new_game"},
    },
};

UnitTestResult check_new_game_chain(const NewGameChainCase& test, CancellableScope& scope){
    std::vector<std::string> steps_run;
    AutosaveSightings sightings;
    PlzaConsoleHooks tools{.sightings = &sightings};
    for (const std::string_view action : {"new_game_intro", "confirm_new_game"}){
        tools.console.emplace(std::string(action), [&steps_run, action](PlzaBackend&, Milliseconds) -> std::optional<Pokemon::StoryFailure>{
            steps_run.emplace_back(action);
            return std::nullopt;
        });
    }
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    std::vector<StoryCheckpoint> rows;
    for (const std::string& id : test.input.rows){
        rows.push_back(builtin_row(tools, id, /*first_step=*/0, /*steps=*/std::numeric_limits<size_t>::max()));
    }
    rows.push_back(builtin_row(tools, "01.01", /*first_step=*/0, /*steps=*/3));
    note_autosaves(rows, sightings);
    Pokemon::FakeStoryBackend backend({.looks = test.input.looks}, scope);
    Pokemon::RunState state;
    CheckpointRunOptions options;
    const std::optional<Pokemon::CheckpointFailure> failure = run_plza_checkpoints(game, rows, options, backend, state, sightings);
    const std::string verdict = verdict_of(failure, rows, /*with_checkpoint=*/true);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    const std::string console_steps = joined(steps_run, " ");
    if (console_steps != test.expectation.console_steps){
        return std::format("expected the console steps \"{}\", got \"{}\"", test.expectation.console_steps, console_steps);
    }
    return true;
}

//  A checkpoint whose last step did or did not save from the menu.
struct MenuSaveCase{
    std::string name;
    struct Input{
        bool menu_saved = false;
    };
    Input input;
    struct Expectation{
        std::string line;
    };
    Expectation expectation;
};

const MenuSaveCase MENU_SAVE_CASES[] = {
    {
        .name = "a menu save in the last step saves the checkpoint",
        .input = {.menu_saved = true},
        .expectation = {.line = "[Save] T: the game autosaved (seen)"},
    },
    {
        .name = "no menu save: not saved",
        .input = {.menu_saved = false},
        .expectation = {.line = "[Save] T: not saved (no autosave seen)"},
    },
};

UnitTestResult check_menu_save(const MenuSaveCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::StoryGame game = plza_story_game(MarkedCheckpoints{}, sightings);
    Pokemon::FakeStoryBackend backend({.looks = {}}, scope);
    StoryCheckpoint row;
    row.id = "T";
    row.steps.push_back(StoryStep{.name = "save"});
    sightings.enter("save");
    if (test.input.menu_saved){
        sightings.saved(backend);
    }
    game.save(backend, row);
    if (std::find(backend.lines().begin(), backend.lines().end(), test.expectation.line) == backend.lines().end()){
        return std::format("expected the line \"{}\" in:\n{}", test.expectation.line, joined(backend.lines(), "\n"));
    }
    return true;
}

struct SightingLook{
    //  Empty: a checkpoint starts.
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
};

UnitTestResult check_sightings(const SightingsCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::FakeStoryBackend backend({.looks = {}}, scope);
    for (const SightingLook& entry : test.input.looks){
        if (entry.step.empty()){
            sightings.finish("");
            sightings.restart();
            continue;
        }
        sightings.enter(entry.step);
        sightings.note(backend, look(/*events=*/{}, entry.badge ? std::vector<std::string_view>{AUTOSAVE_READING} : std::vector<std::string_view>{}, entry.at));
    }
    if (sightings.seen_in() != test.expectation.seen_in){
        return std::format(
            "expected seen in \"{}\", got \"{}\"; log:\n{}",
            test.expectation.seen_in.value_or("none"), sightings.seen_in().value_or("none"), joined(backend.lines(), "\n")
        );
    }
    return true;
}

template <typename Case, UnitTestResult (*check)(const Case&, CancellableScope&)>
class CaseTest : public UnitTest{
public:
    CaseTest(std::string_view kind, Case test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame {}{}", kind, test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check(m_case, scope);
    }
private:
    Case m_case;
};

} // namespace


void add_tests_AutoStoryGame(UnitTestDatabase& database){
    for (const SightingsCase& test : SIGHTINGS_CASES){
        database.add<CaseTest<SightingsCase, check_sightings>>("autosave sightings - ", test);
    }
    for (const MenuSaveCase& test : MENU_SAVE_CASES){
        database.add<CaseTest<MenuSaveCase, check_menu_save>>("menu save - ", test);
    }
    for (const AutosaveStepCase& test : AUTOSAVE_STEP_CASES){
        database.add<CaseTest<AutosaveStepCase, check_autosave_step>>("autosave step - ", test);
    }
    for (const RunCase& test : RUN_CASES){
        database.add<CaseTest<RunCase, check_run>>("run - ", test);
    }
    for (const LoadCase& test : LOAD_CASES){
        database.add<CaseTest<LoadCase, check_load>>("load - ", test);
    }
    for (const ChainCase& test : CHAIN_CASES){
        database.add<CaseTest<ChainCase, check_chain>>("chain - ", test);
    }
    for (const NewGameChainCase& test : NEW_GAME_CHAIN_CASES){
        database.add<CaseTest<NewGameChainCase, check_new_game_chain>>("new game chain - ", test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
