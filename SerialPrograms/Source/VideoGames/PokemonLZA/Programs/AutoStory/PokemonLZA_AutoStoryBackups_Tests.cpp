/*  PokemonLZA AutoStory - Backup saves Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStoryFiles.h"
#include "PokemonLZA_AutoStoryBackups.h"
#include "PokemonLZA_AutoStoryBackups_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::ZonePose;

std::string joined(const std::vector<std::string>& texts, std::string_view separator){
    std::string out;
    for (const std::string& text : texts){
        out += out.empty() ? text : std::format("{}{}", separator, text);
    }
    return out;
}

//  The built-in plans' backup cards, read after 03.08 failed.
struct CardCase{
    std::string name;
    struct Input{
        BackupCard card;
    };
    Input input;
    struct Expectation{
        //  None: no checkpoint starts there, the backup is not loaded.
        std::optional<std::string> checkpoint;
    };
    Expectation expectation;
};

const CardCase CARD_CASES[] = {
    {
        //  console B, 2026-11-16: after 03.08 autosaved, the backup showed
        //  1 registered and loaded at the Center counter.
        .name = "1 registered: 03.06's start at the Center counter",
        .input = {.card = {.registered = 1}},
        .expectation = {.checkpoint = "03.06"},
    },
    {
        .name = "an unknown card: no checkpoint",
        .input = {.card = {.registered = 7}},
        .expectation = {.checkpoint = std::nullopt},
    },
};

//  The built-in row `id`'s index; none when there is no such row.
std::optional<size_t> builtin_row(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = builtin_story_data().checkpoints;
    for (size_t index = 0; index < rows.size(); index++){
        if (rows[index].id == id){
            return index;
        }
    }
    return std::nullopt;
}

//  The card read after 03.08 failed, as a rewind reads it.
UnitTestResult check_card(const CardCase& test){
    const std::vector<StoryCheckpoint>& rows = builtin_story_data().checkpoints;
    const std::optional<size_t> failed = builtin_row("03.08");
    if (!failed){
        return std::string("no 03.08 among the built-in checkpoints");
    }
    const std::optional<size_t> index = checkpoint_at(
        rows, builtin_checkpoint_starts(), *failed, StartSignature{.registered = test.input.card.registered}
    );
    const std::optional<std::string> checkpoint = index ? std::optional<std::string>(rows[*index].id) : std::nullopt;
    if (checkpoint != test.expectation.checkpoint){
        return std::format(
            "expected {}, got {}",
            test.expectation.checkpoint.value_or("none"), checkpoint.value_or("none")
        );
    }
    return true;
}


//  A look with the readings named in `readings`.
Observation<ZonePose> look(std::vector<std::string_view> readings){
    Observation<ZonePose> observation;
    observation.when = WallClock::min();
    for (std::string_view reading : readings){
        observation.readings[std::string(reading)] = 1;
    }
    return observation;
}

//  A checkpoint `id` whose GAME steps run `actions`, each step named for
//  its action.
StoryCheckpoint row(std::string id, std::vector<std::string> actions){
    StoryCheckpoint checkpoint;
    checkpoint.id = std::move(id);
    checkpoint.title = "Test";
    for (std::string& action : actions){
        checkpoint.steps.push_back(StoryStep{.kind = Pokemon::StoryStepKind::GAME, .name = action, .why = "", .action = action, .args = {}});
    }
    return checkpoint;
}

//  03.06 and 03.07 check the platform; 03.08 does, then catches. The run
//  goes from 03.08 to 03.08, as on console B 2026-11-16.
std::vector<StoryCheckpoint> story_rows(){
    return {
        row("03.06", {"wait_for_platform"}),
        row("03.07", {"wait_for_platform"}),
        row("03.08", {"wait_for_platform", "catch_in_zone"}),
    };
}
constexpr size_t RUN_FROM = 2;
constexpr size_t RUN_TO = 2;

struct RewindCase{
    std::string name;
    struct Input{
        //  The badge on the first look: 03.08 autosaves before its catch.
        bool autosave_in_failed_run;
        //  How many catches fail before they pass.
        size_t failed_catches;
        //  What the backup card reads; none: no card read.
        std::optional<BackupCard> card;
        size_t max_rewinds = 2;
        //  What the failed catches fail as.
        std::string_view failure = "lost";
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  The checkpoints run, in order, each "<id> passed" or "<id> failed".
        std::string runs;
        //  What the backup save did, in order: "open", "load", "cancel".
        std::string backup_calls;
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const RewindCase REWIND_CASES[] = {
    {
        //  console B, 2026-11-16: 03.08 autosaved at a catch and failed; the
        //  backup (1 registered) loaded at the Center counter, 03.06's start.
        .name = "03.08 fails after a catch autosave: rewinds to 03.06 and plays on through 03.08",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = BackupCard{.registered = 1}},
        .expectation = {
            .verdict = "done",
            .runs = "03.08 failed, 03.06 passed, 03.07 passed, 03.08 passed",
            .backup_calls = "open, load",
            .line = "[Rewind] backup card: 1 registered: 03.06's start; loading it",
        },
    },
    {
        .name = "a failure with no autosave in it does not rewind",
        .input = {.autosave_in_failed_run = false, .failed_catches = 1, .card = BackupCard{.registered = 1}},
        .expectation = {
            .verdict = "failed at 03.08 catch_in_zone: lost",
            .runs = "03.08 failed",
            .backup_calls = "",
            .line = "[Rewind] 03.08 failed with no autosave in it: no rewind",
        },
    },
    {
        //  console B, 2026-11-16 16:40: a catch at 98 autosaved with the
        //  badge unseen; the next try reloaded off the rocks (read 96).
        .name = "03.08 fails as rocks_not_left with no autosave seen: the save moved, rewinds",
        .input = {.autosave_in_failed_run = false, .failed_catches = 1, .card = BackupCard{.registered = 1}, .failure = "rocks_not_left"},
        .expectation = {
            .verdict = "done",
            .runs = "03.08 failed, 03.06 passed, 03.07 passed, 03.08 passed",
            .backup_calls = "open, load",
        },
    },
    {
        .name = "an unknown backup card is cancelled, never loaded",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = BackupCard{.registered = 7}},
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
        .input = {.autosave_in_failed_run = true, .failed_catches = 9, .card = BackupCard{.registered = 1}},
        .expectation = {
            .verdict = "failed at 03.08 catch_in_zone: lost",
            .runs = "03.08 failed, 03.06 passed, 03.07 passed, 03.08 failed",
            .backup_calls = "open, load",
        },
    },
    {
        .name = "rewinds turned off: stops at the first failure",
        .input = {.autosave_in_failed_run = true, .failed_catches = 1, .card = BackupCard{.registered = 1}, .max_rewinds = 0},
        .expectation = {
            .verdict = "failed at 03.08 catch_in_zone: lost",
            .runs = "03.08 failed",
            .backup_calls = "",
            .line = "[Rewind] 03.08 failed after an autosave, but the run has rewound 0 times already: stop",
        },
    },
};

//  The console-less game with a reload that succeeds, its "catch_in_zone"
//  failing as `failure` its first `failed_catches` times, counted in
//  `catches`. The stand-in runs on the script backend: the real one is
//  console_only, which the registry would refuse before the handler.
Pokemon::StoryGame<ZonePose> rewind_game(
    const StoryData& story, size_t failed_catches, std::string_view failure, AutosaveSightings& sightings, size_t& catches
){
    Pokemon::StoryGame<ZonePose> game = make_plza_story_game_without_console(story, sightings);
    game.reload = [](Pokemon::StoryBackend<ZonePose>&){
        return Pokemon::outcome_ok();
    };
    for (Pokemon::StoryAction<ZonePose>& action : game.actions){
        if (action.spec.name == "catch_in_zone"){
            action.spec.console_only = false;
            action.handler = [&catches, failed_catches, failure](Pokemon::StoryBackend<ZonePose>&, const Pokemon::StoryActionCall&, Pokemon::RunState&){
                return catches++ < failed_catches ? Pokemon::outcome_failed(std::string(failure)) : Pokemon::outcome_ok();
            };
        }
    }
    return game;
}

//  A backup save whose card reads `card`, noting each call in `calls`.
BackupSave recording_backup(std::optional<BackupCard> card, std::vector<std::string>& calls){
    return BackupSave{
        .open_card = [card, &calls](PlzaBackend&){
            calls.push_back("open");
            return card;
        },
        .load = [&calls](PlzaBackend&){
            calls.push_back("load");
            return Pokemon::outcome_ok();
        },
        .cancel = [&calls](PlzaBackend&){
            calls.push_back("cancel");
        },
    };
}

//  "<id> passed" or "<id> failed" for each run of `report`.
std::string describe_runs(const Pokemon::SegmentReport& report){
    std::vector<std::string> runs;
    for (const Pokemon::CheckpointRun& run : report.runs){
        runs.push_back(std::format("{} {}", run.id, run.passed ? "passed" : "failed"));
    }
    return joined(runs, ", ");
}

bool has_line(const std::vector<std::string>& lines, std::string_view wanted){
    for (const std::string& line : lines){
        if (line == wanted){
            return true;
        }
    }
    return false;
}

UnitTestResult check_rewind(const RewindCase& test, CancellableScope& scope){
    const StoryData& story = builtin_story_data();
    AutosaveSightings sightings;
    size_t catches = 0;
    const Pokemon::StoryGame<ZonePose> game = rewind_game(story, test.input.failed_catches, test.input.failure, sightings, catches);
    std::vector<std::string> calls;
    const BackupSave backup = recording_backup(test.input.card, calls);
    std::vector<Observation<ZonePose>> looks;
    if (test.input.autosave_in_failed_run){
        looks.push_back(look({AUTOSAVE_READING}));
    }
    looks.push_back(look({PLATFORM_READING}));
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = looks}, scope);
    Pokemon::RunState state;
    const std::vector<StoryCheckpoint> rows = story_rows();
    const std::vector<CheckpointStart> starts{{.checkpoint = "03.06", .signature = {.registered = 1}}};
    const RewindPlan plan{
        .rows = rows, .first = RUN_FROM, .last = RUN_TO, .attempts_per_checkpoint = 1,
        .starts = starts, .max_rewinds = test.input.max_rewinds,
    };
    const Pokemon::SegmentReport report = run_plza_with_rewinds(game, story, plan, backup, backend, sightings, state);
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {} {}: {}", report.failed_checkpoint, report.failed_event, report.failure.failure);
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:\n{}", test.expectation.verdict, verdict, joined(backend.lines(), "\n"));
    }
    if (describe_runs(report) != test.expectation.runs){
        return std::format("expected runs \"{}\", got \"{}\"", test.expectation.runs, describe_runs(report));
    }
    if (joined(calls, ", ") != test.expectation.backup_calls){
        return std::format("expected backup calls \"{}\", got \"{}\"", test.expectation.backup_calls, joined(calls, ", "));
    }
    if (!test.expectation.line.empty() && !has_line(backend.lines(), test.expectation.line)){
        return std::format("expected the line \"{}\" in:\n{}", test.expectation.line, joined(backend.lines(), "\n"));
    }
    return true;
}


//  The start a run asked for, 03.08, read where the save loaded.
struct StartCase{
    std::string name;
    struct Input{
        StartSignature read = {};
        //  What the backup card reads; none: no card read.
        std::optional<BackupCard> card = std::nullopt;
        size_t max_rewinds = 2;
    };
    Input input;
    struct Expectation{
        //  The checkpoint the run plays from; none: it must stop.
        std::optional<std::string> from = std::nullopt;
        //  What the backup save did, in order: "open", "load", "cancel".
        std::string backup_calls = {};
    };
    Expectation expectation;
};

//  On the street: no talk prompt, no lead's panel. Neither 03.06's start
//  (the Center counter) nor 03.08's (in Wild Zone 1).
const StartSignature ON_THE_STREET{.talk_prompt = false, .wild_zone = false};

const StartCase START_CASES[] = {
    {
        .name = "03.08's own start runs it, no backup opened",
        .input = {.read = {.talk_prompt = false, .wild_zone = true}, .card = BackupCard{.registered = 1}},
        .expectation = {.from = "03.08", .backup_calls = ""},
    },
    {
        //  console B, 2026-11-16 17:24: 03.08's latest save stood where
        //  no checkpoint starts, after a catch's autosave.
        .name = "a start that matches no checkpoint rewinds to the backup and runs from the checkpoint its card names",
        .input = {.read = ON_THE_STREET, .card = BackupCard{.registered = 1}},
        .expectation = {.from = "03.06", .backup_calls = "open, load"},
    },
    {
        .name = "a start that matches no checkpoint, rewinds off: the run stops, no backup opened",
        .input = {.read = ON_THE_STREET, .card = BackupCard{.registered = 1}, .max_rewinds = 0},
        .expectation = {.from = std::nullopt, .backup_calls = ""},
    },
    {
        .name = "a start that matches no checkpoint and a backup card that names none: the card cancelled, the run stops",
        .input = {.read = ON_THE_STREET, .card = BackupCard{.registered = 7}},
        .expectation = {.from = std::nullopt, .backup_calls = "open, cancel"},
    },
};

UnitTestResult check_start(const StartCase& test, CancellableScope& scope){
    std::vector<std::string> calls;
    const BackupSave backup = recording_backup(test.input.card, calls);
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = {}}, scope);
    const std::vector<StoryCheckpoint> rows = story_rows();
    const std::vector<CheckpointStart> starts{
        {.checkpoint = "03.06", .signature = {.registered = 1, .talk_prompt = true, .wild_zone = false}},
        {.checkpoint = "03.08", .signature = {.wild_zone = true}},
    };
    const std::optional<size_t> index = start_or_rewind(backend, rows, RUN_FROM, starts, test.input.read, backup, test.input.max_rewinds);
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


class CardTest : public UnitTest{
public:
    explicit CardTest(CardCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStoryBackups card - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_card(m_case);
    }
private:
    CardCase m_case;
};

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
    for (const CardCase& test : CARD_CASES){
        database.add<CardTest>(test);
    }
    for (const RewindCase& test : REWIND_CASES){
        database.add<RewindTest>(test);
    }
    for (const StartCase& test : START_CASES){
        database.add<StartTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
