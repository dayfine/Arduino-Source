/*  Story Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_AutoStory_Tests.h"
#include "Pokemon_StoryLog.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  A formatter's line, made when the table is built.
struct StoryLogInput{
    std::string name;
    std::string line;
};

//  The exact line: the lab watcher and the engine tests read these.
struct StoryLogExpectation{
    std::string line;
};

// One formatter call and the line it must make.
struct StoryLogCase{
    StoryLogInput input;
    StoryLogExpectation expectation;
};

class Test_StoryLog : public UnitTest{
public:
    Test_StoryLog(StoryLogCase test_case)
        : UnitTest("Pokemon::StoryLog - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (m_case.input.line != m_case.expectation.line){
            return "Expected \"" + m_case.expectation.line + "\", got \"" + m_case.input.line + "\"";
        }
        return true;
    }

private:
    const StoryLogCase m_case;
};

const StoryLogCase STORY_LOG_CASES[] = {
    {
        .input = {.name = "event", .line = event_log_line("00.04", 3, "talk_mom", "start")},
        .expectation = {.line = "[Event] 00.04/3 talk_mom: start"},
    },
    {
        .input = {.name = "checkpoint start", .line = checkpoint_start_line("00.04")},
        .expectation = {.line = "[Checkpoint] 00.04: start"},
    },
    {
        .input = {.name = "checkpoint start with its events", .line = checkpoint_start_line("00.04", 3)},
        .expectation = {.line = "[Checkpoint] 00.04: start, 3 events"},
    },
    {
        .input = {.name = "attempt", .line = checkpoint_attempt_line("00.04", 2)},
        .expectation = {.line = "[Checkpoint] 00.04: attempt 2"},
    },
    {
        .input = {.name = "attempt out of a limit", .line = checkpoint_attempt_line("00.04", 2, 3)},
        .expectation = {.line = "[Checkpoint] 00.04: attempt 2/3"},
    },
    {
        .input = {.name = "one-shot pass", .line = checkpoint_pass_line("00.03", 1)},
        .expectation = {.line = "[Checkpoint] 00.03: passed on attempt 1 (one-shot)"},
    },
    {
        .input = {.name = "pass after one reset", .line = checkpoint_pass_line("00.04", 2)},
        .expectation = {.line = "[Checkpoint] 00.04: passed on attempt 2 (1 reset)"},
    },
    {
        .input = {.name = "pass after resets and recoveries in place", .line = checkpoint_pass_line("00.04", 3, 1)},
        .expectation = {.line = "[Checkpoint] 00.04: passed on attempt 3 (2 resets), 1 in place"},
    },
    {
        .input = {.name = "reset", .line = checkpoint_reset_line("00.04", 2)},
        .expectation = {.line = "[Checkpoint] 00.04: failed, resetting for attempt 2"},
    },
    {
        .input = {.name = "reload within a segment", .line = checkpoint_reset_line("00", "00.02", 2, 3)},
        .expectation = {.line = "[Segment] 00: 00.02 failed, reloading for attempt 2/3"},
    },
};


//  Counts added to fresh stats, in order.
struct StoryCountsInput{
    std::string name;
    std::vector<StoryRunCounts> added;
};

//  The stats after them: counts add up rather than replace.
struct StoryCountsExpectation{
    uint64_t segments;
    uint64_t checkpoints;
    uint64_t resets;
};

// Counts recorded into the AutoStory stats and the totals they must reach.
struct StoryCountsCase{
    StoryCountsInput input;
    StoryCountsExpectation expectation;
};

class Test_StoryCounts : public UnitTest{
public:
    Test_StoryCounts(StoryCountsCase test_case)
        : UnitTest("Pokemon::StoryLog - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        AutoStoryStats stats;
        for (const StoryRunCounts& counts : m_case.input.added){
            record_story_counts(stats, counts);
        }
        const StoryCountsExpectation& expected = m_case.expectation;
        if (stats.m_segment != expected.segments || stats.m_checkpoint != expected.checkpoints || stats.m_reset != expected.resets){
            return "Expected " + std::to_string(expected.segments) + " segments, " + std::to_string(expected.checkpoints)
                + " checkpoints and " + std::to_string(expected.resets) + " resets, got "
                + std::to_string(stats.m_segment) + ", " + std::to_string(stats.m_checkpoint)
                + " and " + std::to_string(stats.m_reset);
        }
        return true;
    }

private:
    const StoryCountsCase m_case;
};

const StoryCountsCase STORY_COUNTS_CASES[] = {
    {
        .input = {.name = "a segment's counts in the stats", .added = {{.segments = 1, .checkpoints = 2, .resets = 3}}},
        .expectation = {.segments = 1, .checkpoints = 2, .resets = 3},
    },
    {
        .input = {
            .name = "a checkpoint loop's counts add up",
            .added = {{.segments = 0, .checkpoints = 1, .resets = 0}, {.segments = 0, .checkpoints = 0, .resets = 1}, {.segments = 0, .checkpoints = 1, .resets = 0}},
        },
        .expectation = {.segments = 0, .checkpoints = 2, .resets = 1},
    },
};

} // namespace


void add_tests_StoryLog(UnitTestDatabase& database){
    for (const StoryLogCase& test_case : STORY_LOG_CASES){
        database.add<Test_StoryLog>(test_case);
    }
    for (const StoryCountsCase& test_case : STORY_COUNTS_CASES){
        database.add<Test_StoryCounts>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
