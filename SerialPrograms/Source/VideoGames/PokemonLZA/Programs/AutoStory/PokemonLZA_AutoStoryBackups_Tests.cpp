/*  PokemonLZA AutoStory - Backup saves Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryBackups.h"
#include "PokemonLZA_AutoStoryBackups_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using namespace std::chrono_literals;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;

std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string out;
    for (const std::string& text : texts){
        out += out.empty() ? text : std::format("{}{}", separator, text);
    }
    return out;
}

ScreenObservation look(std::vector<std::string_view> readings, Milliseconds after = 0ms){
    ScreenObservation observation;
    observation.when = WallClock::min() + after;
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

//  A checkpoint's id and its steps' actions, each step named for its action.
struct TestRow{
    std::string id;
    std::vector<std::string> actions;
};

TestRow row(std::string id, std::vector<std::string> actions){
    return TestRow{.id = std::move(id), .actions = std::move(actions)};
}

//  `rows` with their steps bound to `tools`, which must outlive them.
std::vector<StoryCheckpoint> checkpoints_of(const std::vector<TestRow>& rows, const PlzaStoryTools& tools){
    Pokemon::StoryActions actions = plza_story_actions(tools);
    std::vector<StoryCheckpoint> checkpoints(rows.size());
    for (size_t index = 0; index < rows.size(); index++){
        checkpoints[index].id = rows[index].id;
        checkpoints[index].title = "Test";
        for (const std::string& action : rows[index].actions){
            checkpoints[index].steps.push_back(StoryStep{.name = action, .action = action, .run = actions.at(action)(JsonObject())});
        }
    }
    return checkpoints;
}

std::vector<TestRow> story_rows(){
    return {
        row("03.06", {"wait_for_platform"}),
        row("03.07", {"wait_for_platform"}),
        row("03.08", {"wait_for_platform", "catch_in_zone"}),
    };
}
constexpr size_t RUN_FROM = 2;
constexpr size_t RUN_TO = 2;

struct RewindExpectation{
    std::string verdict;
    //  "<id> passed" or "<id> failed", in order.
    std::string runs;
    //  "open", "load", "cancel", in order.
    std::string backup_calls;
    std::string line = {};
};

struct RewindCase{
    std::string name;
    struct Input{
        //  The badge on the first look: 03.08 autosaves before its catch.
        bool autosave_in_failed_run;
        size_t failed_catches;
        //  None: no card read.
        std::optional<size_t> card;
        size_t max_rewinds = 2;
        std::string_view failure = "lost";
    };
    Input input;
    RewindExpectation expectation;
};

const RewindCase REWIND_CASES[] = {
    {
        .name = "03.08 fails after a catch autosave: rewinds to 03.06 and plays on through 03.08",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = 1},
        .expectation = {
            .verdict = "done",
            .runs = "03.08 failed, 03.06 passed, 03.07 passed, 03.08 passed",
            .backup_calls = "open, load",
            .line = "[Rewind] backup card: 1 registered: 03.06's start; loading it",
        },
    },
    {
        .name = "a failure with no autosave in it does not rewind",
        .input = {.autosave_in_failed_run = false, .failed_catches = 1, .card = 1},
        .expectation = {
            .verdict = "failed at 03.08 catch_in_zone: lost",
            .runs = "03.08 failed",
            .backup_calls = "",
            .line = "[Rewind] 03.08 failed with no autosave in it: no rewind",
        },
    },
    {
        //  A catch can autosave with the badge unseen.
        .name = "03.08 fails as rocks_not_left with no autosave seen: the save moved, rewinds",
        .input = {.autosave_in_failed_run = false, .failed_catches = 1, .card = 1, .failure = "rocks_not_left"},
        .expectation = {
            .verdict = "done",
            .runs = "03.08 failed, 03.06 passed, 03.07 passed, 03.08 passed",
            .backup_calls = "open, load",
        },
    },
    {
        .name = "an unknown backup card is cancelled, never loaded",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = 7},
        .expectation = {
            .verdict = "failed at 03.08 rewind: unknown_backup",
            .runs = "03.08 failed",
            .backup_calls = "open, cancel",
            .line = "[Rewind] backup card: 7 registered: no checkpoint up to 03.08 starts there; B, nothing loaded",
        },
    },
    {
        .name = "no backup card read: cancelled, nothing loaded",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = std::nullopt},
        .expectation = {
            .verdict = "failed at 03.08 rewind: unknown_backup",
            .runs = "03.08 failed",
            .backup_calls = "open, cancel",
        },
    },
    {
        .name = "a second failure with no autosave in it stops: nothing left to rewind for",
        .input = {.autosave_in_failed_run = true, .failed_catches = 9, .card = 1},
        .expectation = {
            .verdict = "failed at 03.08 catch_in_zone: lost",
            .runs = "03.08 failed, 03.06 passed, 03.07 passed, 03.08 failed",
            .backup_calls = "open, load",
        },
    },
    {
        .name = "rewinds turned off: stops at the first failure",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = 1, .max_rewinds = 0},
        .expectation = {
            .verdict = "failed at 03.08 catch_in_zone: lost",
            .runs = "03.08 failed",
            .backup_calls = "",
            .line = "[Rewind] 03.08 failed after an autosave, but the run has rewound 0 times already: stop",
        },
    },
};

//  A stand-in "catch_in_zone" that looks once, then fails its first
//  `failed_catches` times: the real one runs only on a console.
PlzaStoryTools rewind_tools(size_t failed_catches, std::string_view failure, size_t& catches){
    PlzaStoryTools tools;
    tools.console.emplace("catch_in_zone", [&catches, failed_catches, failure](PlzaBackend& backend, Milliseconds) -> std::optional<Pokemon::StoryFailure>{
        backend.observe();
        if (catches++ < failed_catches){
            return Pokemon::StoryFailure{.code = std::string(failure)};
        }
        return std::nullopt;
    });
    return tools;
}

BackupSave recording_backup(std::optional<size_t> card, std::vector<std::string>& calls){
    return BackupSave{
        .open_card = [card, &calls]{
            calls.push_back("open");
            return card;
        },
        .load = [&calls]{
            calls.push_back("load");
            return std::nullopt;
        },
        .cancel = [&calls]{
            calls.push_back("cancel");
        },
    };
}

//  "03.08 failed, 03.08 passed", from the checkpoint lines.
std::string describe_runs(const std::vector<std::string>& lines){
    std::vector<std::string> runs;
    for (const std::string& line : lines){
        const size_t colon = line.find(": ");
        if (!line.starts_with("[Checkpoint] ") || colon == std::string::npos){
            continue;
        }
        const std::string id = line.substr(13, colon - 13);
        const std::string_view text = std::string_view(line).substr(colon + 2);
        if (text.starts_with("passed on attempt")){
            runs.push_back(id + " passed");
        }else if (text.starts_with("failed at")){
            runs.push_back(id + " failed");
        }
    }
    return joined(runs, ", ");
}

//  "done", or "failed at <checkpoint> <step, or rewind>: <code>".
std::string verdict_of(const std::optional<Pokemon::CheckpointFailure>& failure, std::span<const StoryCheckpoint> rows){
    if (!failure){
        return "done";
    }
    std::string event = "rewind";
    for (const StoryCheckpoint& checkpoint : rows){
        if (checkpoint.id == failure->checkpoint_id && failure->event_index){
            event = checkpoint.steps[*failure->event_index].name;
        }
    }
    return std::format("failed at {} {}: {}", failure->checkpoint_id, event, failure->failure.code);
}

bool has_line(const std::vector<std::string>& lines, std::string_view wanted){
    for (const std::string& line : lines){
        if (line == wanted){
            return true;
        }
    }
    return false;
}

//  One in a checkpoint's first 1.5 s is the save before it.
constexpr Milliseconds OWN_BADGE = 2000ms;

std::vector<ScreenObservation> badge_in_first_step(){
    return {look({}), look({AUTOSAVE_READING}, OWN_BADGE), look({PLATFORM_READING}, OWN_BADGE + 1000ms)};
}

//  The backup card reads 1 registered: 03.06's start.
struct RewindRun{
    std::vector<TestRow> rows;
    size_t first;
    size_t last;
    std::vector<ScreenObservation> looks;
    size_t failed_catches;
    std::string_view failure;
    std::optional<size_t> card;
    size_t max_rewinds;
};

UnitTestResult check_run(const RewindRun& run, const RewindExpectation& expected, CancellableScope& scope){
    AutosaveSightings sightings;
    size_t catches = 0;
    PlzaStoryTools tools = rewind_tools(run.failed_catches, run.failure, catches);
    tools.sightings = &sightings;
    std::vector<StoryCheckpoint> rows = checkpoints_of(run.rows, tools);
    note_autosaves(rows, sightings);
    Pokemon::StoryGame game = plza_story_game(builtin_story_data(), sightings);
    game.reload = [](PlzaBackend&, const StoryCheckpoint& failed) -> std::optional<std::string>{
        return failed.id;
    };
    std::vector<std::string> calls;
    const BackupSave backup = recording_backup(run.card, calls);
    Pokemon::FakeStoryBackend backend({.looks = run.looks}, scope);
    Pokemon::RunState state;
    const std::vector<CheckpointStart> starts{{.checkpoint = "03.06", .signature = {.registered = 1}}};
    const RewindPlan plan{
        .rows = rows, .first = run.first, .last = run.last, .segments = {.attempts_per_checkpoint = 1, .starts = starts},
        .max_rewinds = run.max_rewinds,
    };
    const std::optional<Pokemon::CheckpointFailure> failure = run_plza_with_rewinds(game, plan, backup, backend, sightings, state);
    const std::string verdict = verdict_of(failure, rows);
    if (verdict != expected.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", expected.verdict, verdict, joined(backend.lines(), "\n"));
    }
    if (describe_runs(backend.lines()) != expected.runs){
        return std::format("expected runs \"{}\", got \"{}\"", expected.runs, describe_runs(backend.lines()));
    }
    if (joined(calls, ", ") != expected.backup_calls){
        return std::format("expected backup calls \"{}\", got \"{}\"", expected.backup_calls, joined(calls, ", "));
    }
    if (!expected.line.empty() && !has_line(backend.lines(), expected.line)){
        return std::format("expected the line \"{}\" in:\n{}", expected.line, joined(backend.lines(), "\n"));
    }
    return true;
}

UnitTestResult check_rewind(const RewindCase& test, CancellableScope& scope){
    return check_run(
        RewindRun{
            .rows = story_rows(), .first = RUN_FROM, .last = RUN_TO,
            .looks = test.input.autosave_in_failed_run ? badge_in_first_step() : std::vector{look({PLATFORM_READING})},
            .failed_catches = test.input.failed_catches, .failure = test.input.failure,
            .card = test.input.card, .max_rewinds = test.input.max_rewinds,
        },
        test.expectation, scope
    );
}


//  02.05 and 02.06 come before the first backup save, at 03.06's start.
std::vector<TestRow> gate_rows(){
    return {
        row("02.05", {"wait_for_platform", "catch_in_zone"}),
        row("02.06", {"wait_for_platform", "catch_in_zone"}),
        row("03.06", {"wait_for_platform", "catch_in_zone"}),
    };
}

enum class BadgeIn{
    FIRST_STEP,     //  wait_for_platform: mid-checkpoint
    LAST_STEP,      //  catch_in_zone: the checkpoint's end, the next one's start
};

//  The first catch fails after the game autosaved.
struct GateCase{
    std::string name;
    struct Input{
        std::string_view from;
        std::string_view to;
        BadgeIn badge;
    };
    Input input;
    RewindExpectation expectation;
};

const GateCase GATE_CASES[] = {
    {
        .name = "02.05 fails after an autosave mid-checkpoint: no backup opened, the run stops and says where the save is",
        .input = {.from = "02.05", .to = "02.06", .badge = BadgeIn::FIRST_STEP},
        .expectation = {
            .verdict = "failed at 02.05 rewind: no_backup_yet",
            .runs = "02.05 failed",
            .backup_calls = "",
            .line = "[Rewind] no backup save exists before 03.06: the latest save is in 02.05's wait_for_platform",
        },
    },
    {
        .name = "02.05 fails after the autosave in its last step: no backup opened, it reloads that save and runs on from 02.06",
        .input = {.from = "02.05", .to = "02.06", .badge = BadgeIn::LAST_STEP},
        .expectation = {
            .verdict = "done",
            .runs = "02.05 failed, 02.06 passed",
            .backup_calls = "",
            .line = "[Rewind] no backup save exists before 03.06: the latest save is 02.06's start (02.05's autosave in catch_in_zone); loading it",
        },
    },
    {
        .name = "03.06 fails after an autosave: a backup exists from its start, the run rewinds to it",
        .input = {.from = "03.06", .to = "03.06", .badge = BadgeIn::FIRST_STEP},
        .expectation = {
            .verdict = "done",
            .runs = "03.06 failed, 03.06 passed",
            .backup_calls = "open, load",
        },
    },
};

std::optional<size_t> index_of(const std::vector<TestRow>& rows, std::string_view id){
    for (size_t index = 0; index < rows.size(); index++){
        if (rows[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

UnitTestResult check_gate(const GateCase& test, CancellableScope& scope){
    std::vector<TestRow> rows = gate_rows();
    const std::optional<size_t> from = index_of(rows, test.input.from);
    const std::optional<size_t> to = index_of(rows, test.input.to);
    if (!from || !to){
        return std::format("no row {} or {}", test.input.from, test.input.to);
    }
    const std::vector<ScreenObservation> looks = test.input.badge == BadgeIn::FIRST_STEP
        ? badge_in_first_step()
        : std::vector{
            look({}), look({PLATFORM_READING}, 1000ms), look({PLATFORM_READING}, 1300ms),
            look({AUTOSAVE_READING}, OWN_BADGE), look({PLATFORM_READING}, OWN_BADGE + 1000ms),
        };
    return check_run(
        RewindRun{
            .rows = std::move(rows), .first = *from, .last = *to, .looks = looks,
            .failed_catches = 1, .failure = "lost", .card = 1, .max_rewinds = 2,
        },
        test.expectation, scope
    );
}


//  The start a run asked for, read where the save loaded.
struct StartCase{
    std::string name;
    struct Input{
        //  A row of start_rows().
        std::string_view asked = "03.08";
        StartSignature read = {};
        //  None: no card read.
        std::optional<size_t> card = std::nullopt;
        size_t max_rewinds = 2;
    };
    Input input;
    struct Expectation{
        //  None: the run must stop.
        std::optional<std::string> from = std::nullopt;
        std::string backup_calls = {};
    };
    Expectation expectation;
};

//  Neither 03.06's start (the Center counter) nor 03.08's (in Wild Zone 1).
const StartSignature ON_THE_STREET{.talk_prompt = false, .wild_zone = false};

const StartCase START_CASES[] = {
    {
        .name = "03.08's own start runs it, no backup opened",
        .input = {.read = {.talk_prompt = false, .wild_zone = true}, .card = 1},
        .expectation = {.from = "03.08", .backup_calls = ""},
    },
    {
        .name = "a start that matches no checkpoint rewinds to the backup and runs from the checkpoint its card names",
        .input = {.read = ON_THE_STREET, .card = 1},
        .expectation = {.from = "03.06", .backup_calls = "open, load"},
    },
    {
        .name = "a start that matches no checkpoint, rewinds off: the run stops, no backup opened",
        .input = {.read = ON_THE_STREET, .card = 1, .max_rewinds = 0},
        .expectation = {.from = std::nullopt, .backup_calls = ""},
    },
    {
        .name = "a start that matches no checkpoint and a backup card that names none: the card cancelled, the run stops",
        .input = {.read = ON_THE_STREET, .card = 7},
        .expectation = {.from = std::nullopt, .backup_calls = "open, cancel"},
    },
    {
        .name = "02.05's start not matched: no backup exists before 03.06, none opened, the run stops",
        .input = {.asked = "02.05", .read = ON_THE_STREET, .card = 1},
        .expectation = {.from = std::nullopt, .backup_calls = ""},
    },
};

std::vector<TestRow> start_rows(){
    std::vector<TestRow> rows{row("02.05", {"wait_for_platform"})};
    for (TestRow& later : story_rows()){
        rows.push_back(std::move(later));
    }
    return rows;
}

UnitTestResult check_start(const StartCase& test, CancellableScope& scope){
    std::vector<std::string> calls;
    const BackupSave backup = recording_backup(test.input.card, calls);
    Pokemon::FakeStoryBackend backend({.looks = {}}, scope);
    const PlzaStoryTools tools;
    std::vector<StoryCheckpoint> rows = checkpoints_of(start_rows(), tools);
    const std::optional<size_t> asked = index_of(start_rows(), test.input.asked);
    if (!asked){
        return std::format("no row {}", test.input.asked);
    }
    const std::vector<CheckpointStart> starts{
        {.checkpoint = "02.05", .signature = {.talk_prompt = true}},
        {.checkpoint = "03.06", .signature = {.registered = 1, .talk_prompt = true, .wild_zone = false}},
        {.checkpoint = "03.08", .signature = {.wild_zone = true}},
    };
    const RewindPlan plan{.rows = rows, .first = *asked, .last = *asked, .segments = {.starts = starts}, .max_rewinds = test.input.max_rewinds};
    const std::optional<size_t> index = start_or_rewind(backend, plan, test.input.read, backup);
    const std::optional<std::string> from = index ? std::optional<std::string>(rows[*index].id) : std::nullopt;
    if (from != test.expectation.from){
        return std::format(
            "expected the run from {}, got {}; log:\n{}",
            test.expectation.from.value_or("none"), from.value_or("none"), joined(backend.lines(), "\n")
        );
    }
    if (joined(calls, ", ") != test.expectation.backup_calls){
        return std::format("expected backup calls \"{}\", got \"{}\"", test.expectation.backup_calls, joined(calls, ", "));
    }
    return true;
}


class RewindTest : public UnitTest{
public:
    explicit RewindTest(RewindCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryBackups rewind - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_rewind(m_case, scope);
    }
private:
    RewindCase m_case;
};

class GateTest : public UnitTest{
public:
    explicit GateTest(GateCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryBackups first backup - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_gate(m_case, scope);
    }
private:
    GateCase m_case;
};

class StartTest : public UnitTest{
public:
    explicit StartTest(StartCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryBackups start - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_start(m_case, scope);
    }
private:
    StartCase m_case;
};

} // namespace


void add_tests_AutoStoryBackups(UnitTestDatabase& database){
    for (const RewindCase& test : REWIND_CASES){
        database.add<RewindTest>(test);
    }
    for (const GateCase& test : GATE_CASES){
        database.add<GateTest>(test);
    }
    for (const StartCase& test : START_CASES){
        database.add<StartTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
