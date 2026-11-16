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

#include <format>
#include <limits>
#include <optional>
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
        .expectation = {.rows = "01.00:new_game:unverified 01.01:from_save:unverified"},
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
        .name = "an action PLZA does not have fails",
        .input = {.step = {.kind = Pokemon::StoryStepKind::GAME, .name = "m", .why = "", .action = "text_to_choice", .args = {}}},
        .expectation = {.errors = "01.01 step 1 m: unknown action \"text_to_choice\" for GAME"},
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
        .name = "01.01 runs from the platform through the arrival to the mission banner",
        .input = {.checkpoint = "01.01", .first_step = 0, .looks = concat(walk_into_white_fade(), {
            WHITE_FADE,
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(1000)),
            look({ScreenEvent::DIALOG_BOX}, /*readings=*/{}, Milliseconds(1500)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(2000)),
            look(/*events=*/{}, {MISSION_BANNER_READING}, Milliseconds(2250)),
            look(/*events=*/{}, /*readings=*/{}, Milliseconds(2500)),
        })},
        .expectation = {.verdict = "done", .line = "[PLZA mash_to_move_hints] done on mission_banner."},
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
};

UnitTestResult check_run(const RunCase& test, CancellableScope& scope){
    const Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console();
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = test.input.looks}, scope);
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
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
