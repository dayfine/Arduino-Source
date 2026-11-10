/*  AutoStory Stats Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryRun_Tests.h"
#include "Pokemon_AutoStoryStats.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

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
        : UnitTest("Pokemon::AutoStoryStats - " + test_case.input.name)
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


void add_tests_AutoStoryStats(UnitTestDatabase& database){
    for (const StoryCountsCase& test_case : STORY_COUNTS_CASES){
        database.add<Test_StoryCounts>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
