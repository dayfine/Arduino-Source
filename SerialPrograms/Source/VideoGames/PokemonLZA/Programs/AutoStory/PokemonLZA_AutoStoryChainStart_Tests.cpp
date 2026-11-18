/*  PokemonLZA AutoStory - Chained start check Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  run_plza_segments() chaining from one row into the next on a
 *  FakeStoryBackend: the next row's start signature is checked on the live
 *  screen (a scripted reading) before any of its steps runs.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryChainStart_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::Observation;
using Pokemon::StoryCheckpoint;
using Pokemon::ZonePose;

Observation<ZonePose> look_of(std::optional<std::string_view> reading){
    Observation<ZonePose> observation;
    observation.when = WallClock::min();
    if (reading){
        observation.readings[std::string(*reading)] = 1;
    }
    return observation;
}

//  01.01 waits for the platform (two looks, no autosave), then the chain's
//  START_LOOKS looks off the platform.
const std::vector<Observation<ZonePose>> CHAIN_LOOKS = {
    look_of(PLATFORM_READING), look_of(PLATFORM_READING),
    look_of(std::nullopt), look_of(std::nullopt), look_of(std::nullopt), look_of(std::nullopt),
};

//  03.09's start: on the street out of Wild Zone 1's far gate.
const StartSignature OUT_OF_FAR_GATE{.wild_zone = false, .party = 5};


struct ChainStartCase{
    std::string name;
    struct Input{
        //  01.02's start signature; none: its plan names none.
        std::optional<StartSignature> start;
        //  What the live screen reads after 01.01.
        StartSignature screen;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  A log line the run wrote, if any is checked.
        std::string line = {};
    };
    Expectation expectation;
};

const ChainStartCase CHAIN_START_CASES[] = {
    {
        .name = "the live screen shows the row's start: on",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .wild_zone = false, .party = 5}},
        .expectation = {
            .verdict = "done",
            .line = "[Checkpoint] 01.02: START CHECKED: the screen reads (no talk prompt, not in a wild zone, party 5), "
                "its start is (not in a wild zone, party 5); on from the live screen",
        },
    },
    {
        //  console B, 2026-11-18 13:37: 03.08b saved in Wild Zone 1, and
        //  03.09 ran on from there into a building's wall.
        .name = "the live screen is not at the row's start: stops there",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {
            .verdict = "failed at 01.02 chain: chain_off_start",
            .line = "[Checkpoint] 01.02: START CHECK FAILED: the screen reads (no talk prompt, in a wild zone, party 5), "
                "its start is (not in a wild zone, party 5); nothing run from there",
        },
    },
    {
        //  The zone not read, party 5 alone matched 03.09's start.
        .name = "a field of the row's start not read: stops there",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .party = 5}},
        .expectation = {.verdict = "failed at 01.02 chain: chain_off_start"},
    },
    {
        .name = "the backup card's registered is not read on the overworld: on",
        .input = {
            .start = StartSignature{.registered = 1, .talk_prompt = true, .wild_zone = false, .party = 1},
            .screen = {.talk_prompt = true, .wild_zone = false, .party = 1},
        },
        .expectation = {.verdict = "done"},
    },
    {
        .name = "the row names no start: on, unchecked",
        .input = {.start = std::nullopt, .screen = {}},
        .expectation = {
            .verdict = "done",
            .line = "[Checkpoint] 01.02: START UNCHECKED: its plan names no start signature; on from the live screen (autosave before it not seen)",
        },
    },
};

UnitTestResult check_chain_start(const ChainStartCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    const Pokemon::StoryGame<ZonePose> game = plza_story_game(
        /*console=*/{}, Pokemon::StoryRows{}, /*arrow=*/std::nullopt, /*lock_on=*/std::nullopt, sightings
    );
    Pokemon::FakeStoryBackend<ZonePose> backend({.looks = CHAIN_LOOKS}, scope);
    StoryCheckpoint first{.id = "01.01", .title = "Test"};
    first.steps.push_back(Pokemon::StoryStep{
        .kind = Pokemon::StoryStepKind::GAME, .name = "wait_for_platform", .why = "", .action = "wait_for_platform", .args = {},
    });
    const std::vector<StoryCheckpoint> rows{first, StoryCheckpoint{.id = "01.02", .title = "Test"}};
    std::vector<CheckpointStart> starts;
    if (test.input.start){
        starts.push_back(CheckpointStart{.checkpoint = "01.02", .signature = *test.input.start});
    }
    SegmentRunOptions options{.starts = starts};
    options.read_start = [&test](const StartSignature&){
        return test.input.screen;
    };
    Pokemon::RunState state;
    const Pokemon::SegmentReport report = run_plza_segments(game, rows, options, backend, state, sightings);
    const std::string verdict = report.completed
        ? "done"
        : std::format("failed at {} {}: {}", report.failed_checkpoint, report.failed_event, report.failure.failure);
    std::string log;
    bool has_line = test.expectation.line.empty();
    for (const std::string& line : backend.lines()){
        log += std::format("\n{}", line);
        has_line = has_line || line == test.expectation.line;
    }
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:{}", test.expectation.verdict, verdict, log);
    }
    if (!has_line){
        return std::format("expected the line \"{}\" in:{}", test.expectation.line, log);
    }
    return true;
}


class Test_ChainStart : public UnitTest{
public:
    explicit Test_ChainStart(const ChainStartCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStoryGame chain start - {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_chain_start(m_test, scope);
    }

private:
    const ChainStartCase& m_test;
};

} // namespace


void add_tests_AutoStoryChainStart(UnitTestDatabase& database){
    for (const ChainStartCase& test : CHAIN_START_CASES){
        database.add<Test_ChainStart>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
