/*  Max Lair Path Policy - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure tests of the path policy: no hardware, no GUI. Includes a small
 *  Monte Carlo model that compares the two path keeping rules.
 *
 */

#include <cmath>
#include <map>
#include <tuple>
#include <vector>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonSwSh_MaxLair_PathPolicy.h"
#include "PokemonSwSh_MaxLair_PathPolicy_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSwSh{
namespace MaxLairInternal{

namespace{



std::string record_str(const WinRecord& record){
    return std::to_string(record.wins) + "/" + std::to_string(record.runs);
}

PathPolicySettings settings_for(PathKeepingRule rule){
    PathPolicySettings settings;
    settings.rule = rule;
    return settings;
}

const char* rule_str(PathKeepingRule rule){
    return rule == PathKeepingRule::OBSERVED_WIN_RATE ? "observed" : "likely";
}



struct BetaProbabilityCase{
    const char* name;
    double a1;
    double b1;
    //  Unused (0) for an "at least" case.
    double a2;
    double b2;
    //  Unused (-1) for a "greater" case.
    double x;
    double expected;
};

class Test_BetaProbability : public UnitTest{
public:
    Test_BetaProbability(BetaProbabilityCase test)
        : UnitTest(std::string("PokemonSwSh::MaxLair::PathPolicy::BetaProbability - ") + test.name)
        , m_case(test)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        double actual = m_case.x >= 0
            ? probability_beta_at_least(m_case.a1, m_case.b1, m_case.x)
            : probability_beta_greater(m_case.a1, m_case.b1, m_case.a2, m_case.b2);
        if (std::abs(actual - m_case.expected) > 1e-3){
            return UnitTestResult(
                "Expected " + tostr_fixed(m_case.expected, 4) + ", got " + tostr_fixed(actual, 4) + "."
            );
        }
        return UnitTestResult(true);
    }

private:
    BetaProbabilityCase m_case;
};



struct DecideKeepPathCase{
    const char* name;
    PathKeepingRule rule;
    WinRecord path;
    WinRecord new_paths;
    bool expected_keep;
};

class Test_DecideKeepPath : public UnitTest{
public:
    Test_DecideKeepPath(DecideKeepPathCase test)
        : UnitTest(
            std::string("PokemonSwSh::MaxLair::PathPolicy::DecideKeepPath - ") +
            rule_str(test.rule) + " " + test.name
        )
        , m_case(test)
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        PathPolicyDecision decision = decide_keep_path(settings_for(m_case.rule), m_case.path, m_case.new_paths);
        logger.log(decision.reason);
        if (decision.keep != m_case.expected_keep){
            return UnitTestResult(
                std::string("Expected ") + (m_case.expected_keep ? "keep" : "abandon") +
                ", got " + (decision.keep ? "keep" : "abandon") + ". " + decision.reason
            );
        }
        return UnitTestResult(true);
    }

private:
    DecideKeepPathCase m_case;
};



//  Replays the runs of one real session, in order. Each string is one path:
//  the runs played on it before the program moved on. For each run, the test
//  compares both rules' decision with the expected one. The observed rule
//  must reproduce what the program actually did.
struct ReplayCase{
    const char* name;
    std::vector<std::string> paths;
    //  One character per run: K = keep, A = abandon.
    std::string expected_observed;
    std::string expected_likely;
};

class Test_ReplaySession : public UnitTest{
public:
    Test_ReplaySession(ReplayCase test)
        : UnitTest(std::string("PokemonSwSh::MaxLair::PathPolicy::Replay - ") + test.name)
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        std::string actual_observed = replay(logger, PathKeepingRule::OBSERVED_WIN_RATE);
        std::string actual_likely = replay(logger, PathKeepingRule::LIKELY_WIN_RATE);
        if (actual_observed != m_case.expected_observed){
            return UnitTestResult(
                "Observed rule: expected " + m_case.expected_observed + ", got " + actual_observed + "."
            );
        }
        if (actual_likely != m_case.expected_likely){
            return UnitTestResult(
                "Likely rule: expected " + m_case.expected_likely + ", got " + actual_likely + "."
            );
        }
        return UnitTestResult(true);
    }

private:
    std::string replay(Logger& logger, PathKeepingRule rule) const{
        PathPolicySettings settings = settings_for(rule);
        WinRecord new_paths;
        std::string ret;
        for (const std::string& runs : m_case.paths){
            WinRecord path;
            for (char result : runs){
                path.add_run(result == 'W');
                if (path.runs == 1){
                    new_paths.add_run(result == 'W');
                }
                PathPolicyDecision decision = decide_keep_path(settings, path, new_paths);
                ret += decision.keep ? 'K' : 'A';
                logger.log(
                    std::string(rule_str(rule)) + ": " + (decision.keep ? "keep" : "abandon") +
                    " - " + decision.reason
                );
            }
        }
        return ret;
    }

private:
    ReplayCase m_case;
};



//  Monte Carlo model of a session. Each new path gets a win rate drawn
//  uniformly from "path_rates". Each run is won with that rate. After each
//  run, the rule decides to keep the path or move to a new one.
struct ScenarioCase{
    const char* name;
    std::vector<double> path_rates;
    size_t runs_per_session;
    size_t sessions;
    //  The likely rule's win rate minus the observed rule's must be at least this.
    double min_gain;
};

//  SplitMix64. The standard library's distributions differ between
//  compilers, so use our own to get the same numbers everywhere.
class ScenarioRandom{
public:
    ScenarioRandom(uint64_t seed)
        : m_state(seed)
    {}
    uint64_t next(){
        uint64_t z = (m_state += 0x9e3779b97f4a7c15);
        z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
        z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
        return z ^ (z >> 31);
    }
    double uniform(){
        return (double)(next() >> 11) * (1.0 / 9007199254740992.0);
    }

private:
    uint64_t m_state;
};

class Test_Scenario : public UnitTest{
public:
    Test_Scenario(ScenarioCase test)
        : UnitTest(std::string("PokemonSwSh::MaxLair::PathPolicy::Scenario - ") + test.name)
        , m_case(std::move(test))
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        //  Same seed for both rules, so they see the same sequence of draws.
        double observed_rate = simulate(PathKeepingRule::OBSERVED_WIN_RATE);
        double likely_rate = simulate(PathKeepingRule::LIKELY_WIN_RATE);
        std::string summary =
            std::to_string(m_case.sessions) + " sessions of " + std::to_string(m_case.runs_per_session) +
            " runs: observed rule wins " + tostr_fixed(observed_rate, 3) +
            ", likely rule wins " + tostr_fixed(likely_rate, 3) +
            ", gain " + tostr_fixed(likely_rate - observed_rate, 3);
        logger.log(summary);
        if (likely_rate - observed_rate < m_case.min_gain){
            return UnitTestResult(
                "Expected a gain of at least " + tostr_fixed(m_case.min_gain, 3) + ". " + summary
            );
        }
        return UnitTestResult(true);
    }

private:
    double simulate(PathKeepingRule rule) const{
        ScenarioRandom random(1);
        PathPolicySettings settings = settings_for(rule);

        //  The decision only depends on the two records. Cache it: the
        //  integration is the slow part.
        std::map<std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>, bool> cache;
        auto keep = [&](const WinRecord& path, const WinRecord& new_paths){
            auto key = std::make_tuple(path.wins, path.runs, new_paths.wins, new_paths.runs);
            auto iter = cache.find(key);
            if (iter != cache.end()){
                return iter->second;
            }
            bool ret = decide_keep_path(settings, path, new_paths).keep;
            cache.emplace(key, ret);
            return ret;
        };
        auto draw_rate = [&]{
            return m_case.path_rates[(size_t)(random.uniform() * m_case.path_rates.size())];
        };

        uint64_t wins = 0;
        for (size_t session = 0; session < m_case.sessions; session++){
            WinRecord path;
            WinRecord new_paths;
            double rate = draw_rate();
            for (size_t c = 0; c < m_case.runs_per_session; c++){
                bool win = random.uniform() < rate;
                wins += win;
                path.add_run(win);
                if (path.runs == 1){
                    new_paths.add_run(win);
                }
                if (!keep(path, new_paths)){
                    path = WinRecord();
                    rate = draw_rate();
                }
            }
        }
        return (double)wins / (double)(m_case.sessions * m_case.runs_per_session);
    }

private:
    ScenarioCase m_case;
};





void add_beta_probability_tests(UnitTestDatabase& database){
    const BetaProbabilityCase BETA_CASES[] = {
        {.name = "uniform at least 0.75",      .a1 = 1,  .b1 = 1, .a2 = 0, .b2 = 0, .x = 0.75, .expected = 0.25},
        {.name = "one win at least 0.75",      .a1 = 2,  .b1 = 1, .a2 = 0, .b2 = 0, .x = 0.75, .expected = 0.4375},
        {.name = "one loss at least 0.75",     .a1 = 1,  .b1 = 2, .a2 = 0, .b2 = 0, .x = 0.75, .expected = 0.0625},
        {.name = "uniform vs uniform",         .a1 = 1,  .b1 = 1, .a2 = 1, .b2 = 1, .x = -1,   .expected = 0.5},
        {.name = "one win vs one loss",        .a1 = 2,  .b1 = 1, .a2 = 1, .b2 = 2, .x = -1,   .expected = 0.8333},
        {.name = "symmetric",                  .a1 = 7,  .b1 = 9, .a2 = 7, .b2 = 9, .x = -1,   .expected = 0.5},
    };
    for (const BetaProbabilityCase& test : BETA_CASES){
        database.add<Test_BetaProbability>(test);
    }
}

void add_decide_keep_path_tests(UnitTestDatabase& database){
    //  New paths here: 5 wins of 12 first runs, as observed for Xerneas.
    using enum PathKeepingRule;
    const DecideKeepPathCase DECIDE_CASES[] = {
        {.name = "first run won", .rule = OBSERVED_WIN_RATE, .path = {.wins = 1, .runs = 1}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = true},
        {.name = "first run lost", .rule = OBSERVED_WIN_RATE, .path = {.wins = 0, .runs = 1}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = false},
        {.name = "3 of 4", .rule = OBSERVED_WIN_RATE, .path = {.wins = 3, .runs = 4}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = true},
        {.name = "4 of 6", .rule = OBSERVED_WIN_RATE, .path = {.wins = 4, .runs = 6}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = false},
        {.name = "14 of 19", .rule = OBSERVED_WIN_RATE, .path = {.wins = 14, .runs = 19}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = false},
        {.name = "first run won", .rule = LIKELY_WIN_RATE,   .path = {.wins = 1, .runs = 1}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = true},
        {.name = "first run lost", .rule = LIKELY_WIN_RATE,   .path = {.wins = 0, .runs = 1}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = false},
        {.name = "one win one loss", .rule = LIKELY_WIN_RATE,   .path = {.wins = 1, .runs = 2}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = false},
        {.name = "4 of 6", .rule = LIKELY_WIN_RATE,   .path = {.wins = 4, .runs = 6}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = true},
        {.name = "14 of 19", .rule = LIKELY_WIN_RATE,   .path = {.wins = 14, .runs = 19}, .new_paths = {.wins = 5, .runs = 12}, .expected_keep = true},
        {.name = "30 of 50, better than new paths", .rule = LIKELY_WIN_RATE,   .path = {.wins = 30, .runs = 50}, .new_paths = {.wins = 15, .runs = 50}, .expected_keep = true},
        {.name = "20 of 50, like new paths", .rule = LIKELY_WIN_RATE,   .path = {.wins = 20, .runs = 50}, .new_paths = {.wins = 20, .runs = 50}, .expected_keep = false},
    };
    for (const DecideKeepPathCase& test : DECIDE_CASES){
        database.add<Test_DecideKeepPath>(test);
    }
}

void add_replay_tests(UnitTestDatabase& database){
    //  Xerneas, 2026-10-27. The observed rule dropped path A at 14/19 and
    //  path B at 4/6. The likely rule keeps both; the runs after each drop never
    //  happened under it, so only the decisions up to each drop are a fair
    //  comparison.
    database.add<Test_ReplaySession>(ReplayCase{
        .name = "Xerneas 2026-10-27",
        .paths = {"WWWWWLWWWWWWWLWLWLL", "L", "L", "WWWLWL", "L", "L"},
        .expected_observed = "KKKKKKKKKKKKKKKKKKA" "A" "A" "KKKKKA" "A" "A",
        .expected_likely = "KKKKKKKKKKKKKKKKKKK" "A" "A" "KKKKKK" "A" "A",
    });
}

void add_scenario_tests(UnitTestDatabase& database){
    const ScenarioCase SCENARIO_CASES[] = {
        {
            .name = "like Xerneas",
            .path_rates = {0.15, 0.25, 0.35, 0.45, 0.55, 0.75},
            .runs_per_session = 200, .sessions = 500, .min_gain = 0.02,
        },
        {
            .name = "two kinds of path",
            .path_rates = {0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.3, 0.75, 0.75, 0.75},
            .runs_per_session = 200, .sessions = 500, .min_gain = 0.02,
        },
        {
            .name = "narrow",
            .path_rates = {0.3, 0.35, 0.4, 0.45, 0.5, 0.55},
            .runs_per_session = 200, .sessions = 500, .min_gain = 0.0,
        },
        //  Paths near 0.95 exist, so moving on aggressively finds them. The
        //  likely rule settles for 0.8 paths more often and loses a little.
        {
            .name = "wide",
            .path_rates = {0.05, 0.2, 0.4, 0.6, 0.8, 0.95},
            .runs_per_session = 200, .sessions = 500, .min_gain = -0.03,
        },
    };
    for (const ScenarioCase& test : SCENARIO_CASES){
        database.add<Test_Scenario>(test);
    }
}



}



void add_tests_MaxLairPathPolicy(UnitTestDatabase& database){
    add_beta_probability_tests(database);
    add_decide_keep_path_tests(database);
    add_replay_tests(database);
    add_scenario_tests(database);
}



}
}
}
}
