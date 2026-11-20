/*  PokemonLZA AutoStory - Chained start check Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A chained row's start signature is checked on the live screen before
 *  any of its steps runs.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryActionTable.h"
#include "PokemonLZA_AutoStoryAutosaves.h"
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryStarts.h"
#include "PokemonLZA_AutoStoryChainStart_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

using Pokemon::StoryCheckpoint;

ScreenObservation look_of(std::optional<std::string_view> reading){
    ScreenObservation observation;
    observation.when = WallClock::min();
    if (reading){
        observation.readings[std::string(*reading)] = 1;
    }
    return observation;
}

//  01.01's platform, then the looks off it.
const std::vector<ScreenObservation> CHAIN_LOOKS = {
    look_of(CONTROLS_HINT_READING), look_of(CONTROLS_HINT_READING),
    look_of(std::nullopt), look_of(std::nullopt), look_of(std::nullopt), look_of(std::nullopt),
};

//  03.09's start: on the street out of Wild Zone 1's far gate.
const StartSignature OUT_OF_FAR_GATE{.wild_zone = false, .party = 5};


struct ChainStartCase{
    std::string name;
    struct Input{
        //  01.02's; none: its plan names none.
        std::optional<StartSignature> start;
        //  After 01.01.
        StartSignature screen;
    };
    Input input;
    struct Expectation{
        //  "done", or "failed at <checkpoint> <event>: <failure>".
        std::string verdict;
        //  The rows SegmentRunOptions::at_start was called for.
        std::string started;
        //  Empty: not checked.
        std::string line = {};
    };
    Expectation expectation;
};

const ChainStartCase CHAIN_START_CASES[] = {
    {
        //  console B, 2026-11-18 13:37: 03.08b saved in Wild Zone 1, and
        //  03.09 ran on from there into a building's wall.
        .name = "the live screen is not at the row's start: stops there",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .wild_zone = true, .party = 5}},
        .expectation = {
            .verdict = "failed at 01.02 chain: chain_off_start",
            .started = "01.01",
            .line = "[Checkpoint] 01.02: START CHECK FAILED: the screen reads (no talk prompt, in a wild zone, party 5), "
                "its start is (not in a wild zone, party 5); nothing run from there",
        },
    },
    {
        .name = "a field of the row's start not read: stops there",
        .input = {.start = OUT_OF_FAR_GATE, .screen = {.talk_prompt = false, .party = 5}},
        .expectation = {.verdict = "failed at 01.02 chain: chain_off_start", .started = "01.01"},
    },
    {
        .name = "the row names no start: on, unchecked",
        .input = {.start = std::nullopt, .screen = {}},
        .expectation = {
            .verdict = "done",
            .started = "01.01 01.02",
            .line = "[Checkpoint] 01.02: START UNCHECKED: its plan names no start signature; on from the live screen (autosave before it not seen)",
        },
    },
};

UnitTestResult check_chain_start(const ChainStartCase& test, CancellableScope& scope){
    AutosaveSightings sightings;
    Pokemon::StoryGame game = plza_story_game(MarkedCheckpoints{}, sightings);
    Pokemon::FakeStoryBackend backend({.looks = CHAIN_LOOKS}, scope);
    PlzaConsoleHooks tools;
    std::vector<StoryCheckpoint> rows(2);
    rows[0].id = "01.01";
    rows[0].steps.push_back(Pokemon::StoryStep{
        .name = "wait_for_platform", .action = "wait_for_platform", .run = plza_story_actions(tools).at("wait_for_platform")(JsonObject()),
    });
    rows[1].id = "01.02";
    std::vector<CheckpointStart> starts;
    if (test.input.start){
        starts.push_back(CheckpointStart{.checkpoint = "01.02", .signature = *test.input.start});
    }
    CheckpointRunOptions options{.starts = starts};
    options.read_start = [&test](const StartSignature&){
        return test.input.screen;
    };
    std::string started;
    options.at_start = [&started](const StoryCheckpoint& row){
        started += started.empty() ? row.id : std::format(" {}", row.id);
    };
    Pokemon::RunState state;
    const std::optional<Pokemon::CheckpointFailure> failure = run_plza_checkpoints(game, rows, options, backend, state, sightings);
    const std::string verdict = !failure
        ? "done"
        : std::format("failed at {} {}: {}", failure->checkpoint_id, failure->event_index ? "step" : "chain", failure->failure.code);
    std::string log;
    bool has_line = test.expectation.line.empty();
    for (const std::string& line : backend.lines()){
        log += std::format("\n{}", line);
        has_line = has_line || line == test.expectation.line;
    }
    if (verdict != test.expectation.verdict){
        return std::format("expected \"{}\", got \"{}\"; log:{}", test.expectation.verdict, verdict, log);
    }
    if (started != test.expectation.started){
        return std::format("expected at_start for \"{}\", got \"{}\"", test.expectation.started, started);
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
