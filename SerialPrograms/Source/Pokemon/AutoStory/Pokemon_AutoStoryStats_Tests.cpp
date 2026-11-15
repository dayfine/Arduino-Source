/*  AutoStory Stats Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon_AutoStoryStats.h"
#include "Pokemon_AutoStoryStats_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

//  A stats file line, as loaded from disk.
struct AutoStoryStatsInput{
    std::string saved_line;
};

//  The stats line written back after loading it.
struct AutoStoryStatsExpectation{
    std::string line;
};

struct AutoStoryStatsScenario{
    std::string name;
    AutoStoryStatsInput input;
    AutoStoryStatsExpectation expectation;
};

class Test_AutoStoryStatsLoad : public UnitTest{
public:
    Test_AutoStoryStatsLoad(AutoStoryStatsScenario scenario)
        : UnitTest(std::format("Pokemon::AutoStoryStats - {}", scenario.name))
        , m_scenario(std::move(scenario))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        AutoStoryStats stats;
        stats.parse_and_append_line(m_scenario.input.saved_line);
        const std::string got = stats.to_str(StatsTracker::SAVE_TO_STATS_FILE);
        if (got != m_scenario.expectation.line){
            return std::format("Expected \"{}\", got \"{}\"", m_scenario.expectation.line, got);
        }
        return true;
    }

private:
    const AutoStoryStatsScenario m_scenario;
};

}  // namespace



void add_tests_AutoStoryStats(UnitTestDatabase& database){
    const std::vector<AutoStoryStatsScenario> load_cases{
        {
            .name = "old labels fill the new ones",
            .input = {.saved_line = "Checkpoint: 12 - Segment: 3 - Reset: 1"},
            .expectation = {.line = "Runs: 0 - Segments Started: 3 - Checkpoints Started: 12 - Resets: 1"},
        },
        {
            .name = "new labels load as they are",
            .input = {.saved_line = "Runs: 4 - Segments Started: 3 - Checkpoints Started: 1,234 - Resets: 2"},
            .expectation = {.line = "Runs: 4 - Segments Started: 3 - Checkpoints Started: 1,234 - Resets: 2"},
        },
        {
            .name = "old and new labels add up",
            .input = {.saved_line = "Checkpoint: 10 - Checkpoints Started: 5 - Runs: 1"},
            .expectation = {.line = "Runs: 1 - Segments Started: 0 - Checkpoints Started: 15 - Resets: 0"},
        },
    };
    for (const AutoStoryStatsScenario& scenario : load_cases){
        database.add<Test_AutoStoryStatsLoad>(scenario);
    }
}



}  // namespace Pokemon
}  // namespace PokemonAutomation
