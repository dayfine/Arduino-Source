/*  Sim Seeds Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The seeded Monte Carlo rows on a fake row: which seeds run, the
 *  failures as sweep variants, and the mean and worst of each measure.
 *
 */

#include <cstdint>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon_SimSeeds.h"
#include "Pokemon_SimSeeds_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  Keeps every line logged, for a test to compare.
class LinesLogger : public Logger{
public:
    using Logger::log;
    virtual void log(const std::string& msg, Color) override{
        lines.push_back(msg);
    }

    std::vector<std::string> lines;
};

//  One seed's outcome in a fake Monte Carlo row.
struct FakeSeed{
    std::optional<std::string> failure;
    std::map<std::string, double> measures;
};

//  Seeds 1 to `last`.
std::vector<uint32_t> every_seed_to(uint32_t last){
    std::vector<uint32_t> out;
    for (uint32_t seed = 1; seed <= last; seed++){
        out.push_back(seed);
    }
    return out;
}

struct RunSeedsCase{
    struct Input{
        //  Unset: the default count.
        std::optional<uint32_t> seeds;
        //  By seed; a seed not given passes with no measures.
        std::map<uint32_t, FakeSeed> outcomes;
    };
    struct Expectation{
        uint32_t seeds;
        std::vector<uint32_t> ran;
        std::vector<SweepFailure> failures;
        std::map<std::string, SeedStats> stats;
        std::vector<std::string> log;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

const RunSeedsCase RUN_SEEDS_CASES[] = {
    {
        .name = "seeds run from 1; mean and worst of the passed seeds",
        .input = {.seeds = 4, .outcomes = {
            {1, {.failure = std::nullopt, .measures = {{"stops", 2}, {"s", 10}}}},
            {2, {.failure = "gave up after 9 stops.", .measures = {{"stops", 9}}}},
            {3, {.failure = std::nullopt, .measures = {{"stops", 4}, {"s", 12}}}},
            {4, {.failure = std::nullopt, .measures = {{"stops", 0}}}},
        }},
        .expectation = {
            .seeds = 4,
            .ran = {1, 2, 3, 4},
            .failures = {{.step = "", .variant = "seed 2", .stopped = "gave up after 9 stops."}},
            .stats = {{"s", {.mean = 11, .worst = 12}}, {"stops", {.mean = 2, .worst = 4}}},
            .log = {"Seeds 02.02 wild battles: passed 3/4; s mean 11.0, worst 12.0; stops mean 2.0, worst 4.0"},
        },
    },
    {
        .name = "no seed passes: no stats",
        .input = {.seeds = 2, .outcomes = {
            {1, {.failure = "a", .measures = {{"stops", 1}}}},
            {2, {.failure = "b", .measures = {}}},
        }},
        .expectation = {
            .seeds = 2,
            .ran = {1, 2},
            .failures = {{.step = "", .variant = "seed 1", .stopped = "a"}, {.step = "", .variant = "seed 2", .stopped = "b"}},
            .stats = {},
            .log = {"Seeds 02.02 wild battles: passed 0/2"},
        },
    },
    {
        .name = "30 seeds unless a row asks for others",
        .input = {.seeds = std::nullopt, .outcomes = {}},
        .expectation = {
            .seeds = 30,
            .ran = every_seed_to(30),
            .failures = {},
            .stats = {},
            .log = {"Seeds 02.02 wild battles: passed 30/30"},
        },
    },
};

bool same_failures(const std::vector<SweepFailure>& a, const std::vector<SweepFailure>& b){
    if (a.size() != b.size()){
        return false;
    }
    for (size_t c = 0; c < a.size(); c++){
        if (a[c].step != b[c].step || a[c].variant != b[c].variant || a[c].stopped != b[c].stopped){
            return false;
        }
    }
    return true;
}

bool same_stats(const std::map<std::string, SeedStats>& a, const std::map<std::string, SeedStats>& b){
    if (a.size() != b.size()){
        return false;
    }
    for (const auto& [measure, stats] : b){
        const auto actual = a.find(measure);
        if (actual == a.end() || actual->second.mean != stats.mean || actual->second.worst != stats.worst){
            return false;
        }
    }
    return true;
}

class Test_RunSeeds : public UnitTest{
public:
    explicit Test_RunSeeds(RunSeedsCase test_case)
        : UnitTest(std::format("Pokemon::Navigation - Sim seeds: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LinesLogger logger;
        std::vector<uint32_t> ran;
        const std::function<SeedRun(uint32_t)> run_one = [&](uint32_t seed){
            ran.push_back(seed);
            const auto outcome = m_case.input.outcomes.find(seed);
            if (outcome == m_case.input.outcomes.end()){
                return SeedRun{};
            }
            return SeedRun{.failure = outcome->second.failure, .measures = outcome->second.measures};
        };
        const SeedsResult result = m_case.input.seeds
            ? run_seeds(logger, "02.02 wild battles", run_one, *m_case.input.seeds)
            : run_seeds(logger, "02.02 wild battles", run_one);
        const RunSeedsCase::Expectation& expected = m_case.expectation;
        if (ran != expected.ran || result.seeds != expected.seeds){
            return std::format("ran {} seeds, reported {}, expected {}.", ran.size(), result.seeds, expected.seeds);
        }
        if (!same_failures(result.failures, expected.failures)){
            return std::format("failures: expected {}, got {}.", expected.failures.size(), result.failures.size());
        }
        if (!same_stats(result.stats, expected.stats)){
            return std::format("stats: expected {} measures, got {}.", expected.stats.size(), result.stats.size());
        }
        if (logger.lines != expected.log){
            return std::format("log: expected \"{}\", got \"{}\".", expected.log.at(0), logger.lines.empty() ? "" : logger.lines[0]);
        }
        return true;
    }

private:
    RunSeedsCase m_case;
};

} // namespace


void add_tests_SimSeeds(UnitTestDatabase& database){
    for (const RunSeedsCase& item : RUN_SEEDS_CASES){
        database.add<Test_RunSeeds>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
