/*  RNG Plan Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_RngPlanTestTargets.h"
#include "PokemonFRLG_RngPlanInputs.h"
#include "PokemonFRLG_RngPlanLog.h"
#include "PokemonFRLG_RngPlanLog_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace Pokemon;
using namespace RngPlanTestTargets;


namespace{

struct RngPlanLogInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct RngPlanLogExpectation{
    const char* description;
};

struct RngPlanLogTestCase{
    RngPlanLogInput input;
    RngPlanLogExpectation expectation;
};

class Test_RngPlanLog : public UnitTest{
public:
    explicit Test_RngPlanLog(RngPlanLogTestCase scenario)
        : UnitTest("PokemonFRLG::RngPlanLog - " + scenario.input.name)
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
    RngPlanLogTestCase m_scenario;
};


//  The "RngPlan:" lines the candidate odds are rebuilt from.
UnitTestResult test_log_lines(Logger&){
    RngAttempt attempt{make_target(1, 50, {0, 1}), {100, 200}};
    attempt.target.seed.seed = 0x2ef6;
    attempt.target.advances = 29364;
    struct Case{
        std::string got;
        std::string expected;
    };
    const Case cases[] = {
        {.got = rng_log_tid(43771, true), .expected = "RngPlan: tid=43771 source=card"},
        {.got = rng_log_tid(43771, false), .expected = "RngPlan: tid=43771 source=option"},
        {.got = rng_log_sid_helper(43771, 3001, CANDIDATES), .expected = "RngPlan: sid_helper tid=43771 target_advances=3001 candidates=100,200,300"},
        {.got = rng_log_attempt(0, attempt), .expected = std::format(
            "RngPlan: attempt=1 seed=2EF6 advances=29364 sids=100,200 seed_wait_ms=49155 ingame_ms={}", starter_rng_ingame_ms(29364)
        )},
        {
            .got = rng_log_result(RngResultLog{
                .index = 1, .try_number = 2, .outcome = RngAttemptOutcome::HIT_NOT_SHINY, .detail = "target_hit",
                .pokemon = attempt.target.pokemon, .next = RngPlanNext::NEXT_ATTEMPT,
            }),
            .expected = std::format(
                "RngPlan: result attempt=2 try=2 outcome=hit_not_shiny starter_rng=target_hit nature={} ivs={} pid={:08X} next=next_attempt",
                nature_to_string(attempt.target.pokemon.nature), ivs_to_string(attempt.target.pokemon.ivs), attempt.target.pokemon.pid
            ),
        },
        {
            .got = rng_log_result(RngResultLog{
                .index = 0, .try_number = 1, .outcome = RngAttemptOutcome::MISSED, .detail = "out_of_resets",
                .pokemon = std::nullopt, .next = RngPlanNext::RETRY,
            }),
            .expected = "RngPlan: result attempt=1 try=1 outcome=missed starter_rng=out_of_resets nature=- ivs=- pid=- next=retry",
        },
        {
            .got = rng_log_result(RngResultLog{
                .index = 2, .try_number = 3, .outcome = RngAttemptOutcome::SHINY_OFF_TARGET, .detail = "shiny_off_target",
                .pokemon = std::nullopt, .next = RngPlanNext::STOP,
            }),
            .expected = "RngPlan: result attempt=3 try=3 outcome=shiny_off_target starter_rng=shiny_off_target nature=- ivs=- pid=- next=stop",
        },
        {.got = rng_log_confirmed(43771, 300, CANDIDATES), .expected = "RngPlan: confirmed tid=43771 sid=300 candidate=3 of=3"},
        {.got = rng_log_confirmed(43771, 7, CANDIDATES), .expected = "RngPlan: confirmed tid=43771 sid=7 candidate=0 of=3"},
    };
    for (const Case& item : cases){
        if (item.got != item.expected){
            return "Expected \"" + item.expected + "\", got \"" + item.got + "\".";
        }
    }
    return true;
}

const RngPlanLogTestCase RNG_PLAN_LOG_CASES[] = {
    {
        .input = {.name = "RngPlan log lines", .body = test_log_lines},
        .expectation = {.description = "verifies structured RNG plan log lines formatting"},
    },
};

} // namespace


void add_tests_RngPlanLog(UnitTestDatabase& database){
    for (const auto& scenario : RNG_PLAN_LOG_CASES){
        database.add<Test_RngPlanLog>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
