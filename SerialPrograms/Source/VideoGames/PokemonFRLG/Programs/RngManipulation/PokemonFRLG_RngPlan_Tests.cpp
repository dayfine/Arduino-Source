/*  RNG Plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The attempt order on made-up targets; SID Helper's candidates and a
 *  Starter RNG plan from the FireRed seed list.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_RngPlanTestTargets.h"
#include "PokemonFRLG_RngPlan.h"
#include "PokemonFRLG_RngPlanLog.h"
#include "PokemonFRLG_RngPlan_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;
using namespace RngPlanTestTargets;


namespace{

struct RngPlanInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct RngPlanExpectation{
    std::string_view description;
};

struct RngPlanTestCase{
    RngPlanInput input;
    RngPlanExpectation expectation;
};

class Test_RngPlan : public UnitTest{
public:
    explicit Test_RngPlan(RngPlanTestCase scenario)
        : UnitTest("PokemonFRLG::RngPlan - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_scenario.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    RngPlanTestCase m_scenario;
};


UnitTestResult test_sid_helper_candidates(Logger&){
    const uint16_t tid = 43771;
    const uint32_t target_advances = 3001;
    const std::vector<SidCandidate> candidates = sid_helper_candidates(tid, target_advances, 5);

    //  The SID is the top half of the state after the TID seed is advanced
    //  once, then "advances" more times.
    auto sid_at = [&](uint32_t advances){
        uint32_t state = tid;
        for (uint32_t c = 0; c <= advances; c++){
            state = lcrng_next(state);
        }
        return uint16_t(state >> 16);
    };
    const std::vector<uint32_t> advances = {2997, 2999, 3001, 3003, 3005};
    const std::vector<double> weights = {1, 2, 3, 2, 1};
    if (candidates.size() != advances.size()){
        return std::format("Expected {} candidates, got {}.", advances.size(), candidates.size());
    }
    for (size_t c = 0; c < candidates.size(); c++){
        if (candidates[c].sid != sid_at(advances[c]) || candidates[c].weight != weights[c]){
            return std::format(
                "Candidate {}: expected SID {} weight {}, got SID {} weight {}.",
                c, sid_at(advances[c]), weights[c], candidates[c].sid, candidates[c].weight
            );
        }
    }

    const std::vector<SidCandidate> even = sid_helper_candidates(tid, target_advances, 4);
    const std::vector<double> even_weights = {1, 2, 3, 2};
    for (size_t c = 0; c < even.size() && c < even_weights.size(); c++){
        if (even.size() != even_weights.size() || even[c].sid != candidates[c].sid || even[c].weight != even_weights[c]){
            return std::format("Four candidates: expected the first four of five, weighing 1, 2, 3, 2; candidate {} differs.", c);
        }
    }
    return true;
}


struct PlanCase{
    std::string name;
    std::vector<RngTarget> targets;
    bool shiny_required;
    size_t max_attempts;
    std::vector<uint32_t> states;
    std::vector<std::vector<uint16_t>> sids;
};
std::vector<PlanCase> plan_cases(){
    const std::vector<RngTarget> no_300 = {TARGETS[0], TARGETS[2], TARGETS[3]};
    return {
        {
            .name = "two candidates in one target first, then the last",
            .targets = TARGETS, .shiny_required = true, .max_attempts = 5,
            .states = {1, 2}, .sids = {{100, 200}, {300}},
        },
        {
            .name = "capped at one attempt",
            .targets = TARGETS, .shiny_required = true, .max_attempts = 1,
            .states = {1}, .sids = {{100, 200}},
        },
        {
            .name = "no target shiny for SID 300",
            .targets = no_300, .shiny_required = true, .max_attempts = 5,
            .states = {1}, .sids = {{100, 200}},
        },
        {
            .name = "shiny not required: the quickest",
            .targets = TARGETS, .shiny_required = false, .max_attempts = 5,
            .states = {4}, .sids = {{}},
        },
        {
            .name = "no targets",
            .targets = {}, .shiny_required = true, .max_attempts = 5,
            .states = {}, .sids = {},
        },
    };
}

UnitTestResult test_plan_order(Logger&){
    for (const PlanCase& item : plan_cases()){
        const std::vector<RngAttempt> attempts = plan_rng_attempts(item.targets, CANDIDATES, item.shiny_required, item.max_attempts);
        std::vector<uint32_t> states;
        std::vector<std::vector<uint16_t>> sids;
        for (const RngAttempt& attempt : attempts){
            states.push_back(attempt.target.state);
            sids.push_back(attempt.sids);
        }
        if (states != item.states || sids != item.sids){
            std::string got;
            for (size_t c = 0; c < attempts.size(); c++){
                got += std::format(" target {} {}", states[c], join(sids[c]));
            }
            return item.name + ": got" + (got.empty() ? " no attempts" : got) + ".";
        }
    }
    return true;
}


//  A plan from the FireRed (English) list: every attempt starts from a seed
//  Starter RNG finds, takes at least its smallest Advances, and is shiny for
//  the SIDs it lists, which no earlier attempt listed.
//  Console B, 2026-11-02, FireRed English, Mono, Help: Starter RNG refused
//  both of these as title screen waits under 30 s. AutoStory's plan for TID
//  55740 started with 11A3 at 945 advances (29,628 ms after Starter RNG's
//  offsets); StarterRng by hand with 405C at 990 advances listed 30,488 ms
//  from the table, 29,643 ms after them. Neither seed may start an attempt.
UnitTestResult test_starter_plan_seed_delay(Logger&){
    struct Case{
        uint16_t seed;
        uint64_t table_ms;
    };
    const Case cases[] = {
        {.seed = 0x11a3, .table_ms = 30473},
        {.seed = 0x405c, .table_ms = 30488},
    };
    const SeedsDatabase database(seeds_json_path(true, Language::English));
    const std::vector<InitialSeedEntry> seeds = initial_seed_entries(database, SoundSetting::Mono, "Help", true);
    for (const Case& item : cases){
        auto entry = std::find_if(seeds.begin(), seeds.end(), [&](const InitialSeedEntry& seed){ return seed.seed == item.seed; });
        if (entry == seeds.end() || entry->seed_time_ms != item.table_ms){
            return std::format("Expected seed {:04X} at {} ms in the FireRed English list (Mono, Help).", item.seed, item.table_ms);
        }
        if (starter_rng_can_wait_for(*entry)){
            return std::format("Seed {:04X} ({} ms): expected too early for Starter RNG's title screen.", item.seed, item.table_ms);
        }
    }

    const uint16_t tid = 55740;
    const std::vector<RngAttempt> attempts = plan_starter_rng(StarterRngPlanInputs{
        .firered = true,
        .language = Language::English,
        .sound = SoundSetting::Mono,
        .tid = tid,
        .candidates = sid_helper_candidates(tid, 3001, 5),
        .filters = {},
        .max_attempts = 5,
    });
    if (attempts.empty()){
        return "Expected attempts for TID 55740, got none.";
    }
    for (size_t c = 0; c < attempts.size(); c++){
        const InitialSeedEntry& seed = attempts[c].target.seed;
        if (seed.seed == 0x11a3 || seed.seed == 0x405c || starter_rng_seed_wait_ms(seed) < 31500){
            return std::format("Attempt {}: seed {:04X} at {} ms is too early for the title screen.", c + 1, seed.seed, seed.seed_time_ms);
        }
        if (attempts[c].target.advances < 1660){
            return std::format("Attempt {}: {} advances leave no room for the in-game calibration.", c + 1, attempts[c].target.advances);
        }
    }
    return true;
}

//  Starter RNG's in-game wait: 7,500 ms at its 940-advance minimum; console
//  B's B579 target at 967 advances was 7,725 ms, which one miss's
//  calibration (-720) cut under the minimum. Targets start 720 advances on.
UnitTestResult test_starter_ingame_wait(Logger&){
    struct Case{
        uint64_t advances;
        int64_t ms;
    };
    const Case cases[] = {
        {.advances = 940, .ms = 7500},
        {.advances = 967, .ms = 7725},
        {.advances = 1660, .ms = 13500},
    };
    for (const Case& item : cases){
        if (starter_rng_ingame_ms(item.advances) != item.ms){
            return std::format("{} advances: expected {} ms in game, got {}.", item.advances, item.ms, starter_rng_ingame_ms(item.advances));
        }
    }
    if (STARTER_RNG_PLAN_MIN_ADVANCES != 1660){
        return std::format("Expected the plan's minimum at 1660 advances, got {}.", STARTER_RNG_PLAN_MIN_ADVANCES);
    }
    return true;
}

UnitTestResult test_starter_plan(Logger&){
    const uint16_t tid = 43771;
    StarterRngPlanInputs inputs{
        .firered = true,
        .language = Language::English,
        .sound = SoundSetting::Mono,
        .tid = tid,
        .candidates = sid_helper_candidates(tid, 3001, 5),
        .filters = {},
        .max_attempts = 5,
    };
    const std::vector<RngAttempt> attempts = plan_starter_rng(inputs);
    if (attempts.empty()){
        return "Expected attempts for TID 43771 with five SID candidates, got none.";
    }

    const SeedsDatabase database(seeds_json_path(true, Language::English));
    std::vector<uint16_t> listed;
    for (size_t c = 0; c < attempts.size(); c++){
        const RngAttempt& attempt = attempts[c];
        if (!database.find_seed(attempt.target.seed.seed, SoundSetting::Mono, 5).found){
            return std::format("Attempt {}: Starter RNG can't find Target Seed {:04X}.", c + 1, attempt.target.seed.seed);
        }
        if (attempt.target.advances < STARTER_RNG_PLAN_MIN_ADVANCES){
            return std::format("Attempt {}: Advances {} below the plan's {}.", c + 1, attempt.target.advances, STARTER_RNG_PLAN_MIN_ADVANCES);
        }
        if (attempt.sids.empty()){
            return std::format("Attempt {}: shiny for no SID candidate.", c + 1);
        }
        for (uint16_t sid : attempt.sids){
            if (shiny_type(attempt.target.pokemon.pid, tid, sid) == AdvShinyType::Normal){
                return std::format("Attempt {}: PID {:08X} not shiny for SID {}.", c + 1, attempt.target.pokemon.pid, sid);
            }
            if (std::find(listed.begin(), listed.end(), sid) != listed.end()){
                return std::format("Attempt {}: SID {} already listed by an earlier attempt.", c + 1, sid);
            }
            listed.push_back(sid);
        }
    }
    if (rng_attempt_lines(attempts).size() != attempts.size()){
        return "Expected one line per attempt.";
    }
    return true;
}

const RngPlanTestCase RNG_PLAN_CASES[] = {
    {
        .input = {.name = "SID Helper's candidates and weights", .body = test_sid_helper_candidates},
        .expectation = {.description = "evaluates SID Helper candidates and candidate weighting"},
    },
    {
        .input = {.name = "attempt order", .body = test_plan_order},
        .expectation = {.description = "validates chronological ordering of plan attempts"},
    },
    {
        .input = {.name = "Starter RNG plan from the FireRed list", .body = test_starter_plan},
        .expectation = {.description = "builds Starter RNG plan from FireRed seed list"},
    },
    {
        .input = {.name = "Starter RNG plan: no seed too early for the title screen", .body = test_starter_plan_seed_delay},
        .expectation = {.description = "filters out seeds too early for title screen timing"},
    },
    {
        .input = {.name = "Starter RNG plan: room for the in-game calibration", .body = test_starter_ingame_wait},
        .expectation = {.description = "allows sufficient margin for in-game calibration advances"},
    },
};

} // namespace


void add_tests_RngPlan(UnitTestDatabase& database){
    for (const auto& scenario : RNG_PLAN_CASES){
        database.add<Test_RngPlan>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
