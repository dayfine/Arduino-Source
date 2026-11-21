/*  Story Run Counts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_AutoStoryStats.h"
#include "Pokemon/AutoStory/Types/Pokemon_StorySegmentTypes.h"
#include "Pokemon_StoryRunCounts_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

// Counts recorded into the AutoStory stats and the totals they must reach.
struct StoryCountsTestCase{
    //  Counts added to fresh stats, in order.
    std::string name;
    struct Input{
        std::vector<StoryRunCounts> added;
    };

    //  The stats after them: counts add up rather than replace.
    struct Expectation{
        uint64_t runs;
        uint64_t checkpoints;
        uint64_t resets;
    };

    Input input;
    Expectation expectation;
};

class Test_StoryCounts : public UnitTest{
public:
    Test_StoryCounts(StoryCountsTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryRunCounts - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        StoryRunStats stats;
        for (const StoryRunCounts& counts : m_case.input.added){
            record_story_counts(counts, stats);
        }
        const StoryCountsTestCase::Expectation& expected = m_case.expectation;
        if (stats.m_runs != expected.runs || stats.m_checkpoints_passed != expected.checkpoints || stats.m_resets != expected.resets){
            return std::format(
                "Expected {} runs, {} checkpoints passed and {} resets, got {}, {} and {}",
                expected.runs, expected.checkpoints, expected.resets,
                stats.m_runs.load(), stats.m_checkpoints_passed.load(), stats.m_resets.load()
            );
        }
        return true;
    }

private:
    const StoryCountsTestCase m_case;
};

const StoryCountsTestCase STORY_COUNTS_CASES[] = {
    {
        .name = "a run's counts in the stats", .input = {.added = {{.runs = 1, .checkpoints = 2, .resets = 3}}},
        .expectation = {.runs = 1, .checkpoints = 2, .resets = 3},
    },
    {
        .name = "a checkpoint loop's counts add up",
        .input = {
            .added = {{.runs = 0, .checkpoints = 1, .resets = 0}, {.runs = 0, .checkpoints = 0, .resets = 1}, {.runs = 0, .checkpoints = 1, .resets = 0}},
        },
        .expectation = {.runs = 0, .checkpoints = 2, .resets = 1},
    },
};

} // namespace


void add_tests_StoryRunCounts(UnitTestDatabase& database){
    for (const StoryCountsTestCase& test_case : STORY_COUNTS_CASES){
        database.add<Test_StoryCounts>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
