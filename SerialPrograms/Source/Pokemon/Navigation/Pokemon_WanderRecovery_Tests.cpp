/*  Wanderer Recovery Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_WanderRecovery.h"
#include "Pokemon_WanderRecovery_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


enum class WanderCallKind{
    WALL,
    MID_LEG,
    UNMOVED,
};

struct WanderStep{
    WanderCallKind kind = WanderCallKind::WALL;
    bool reset_before = false;
    WallWanderInput wall{};
    MidLegWanderInput mid_leg{};
    bool unmoved_blocked = false;
};

struct StepOutcome{
    int decision = 0;
    size_t waits_used = 0;
};

struct WanderRecoveryCase{
    std::string name;
    struct Input{
        WanderWaitPolicy policy{};
        std::vector<WanderStep> steps;
    };
    struct Expectation{
        std::vector<StepOutcome> outcomes;
    };
    Input input{};
    Expectation expectation{};
};


int run_step(
    const WanderWaitPolicy& policy,
    const WanderStep& step,
    WanderWaitState& state
){
    if (step.reset_before){
        state = WanderWaitState{};
    }
    switch (step.kind){
    case WanderCallKind::WALL:
        return static_cast<int>(decide_wall_wander(policy, state, step.wall));
    case WanderCallKind::MID_LEG:
        return static_cast<int>(decide_mid_leg_wander(policy, state, step.mid_leg));
    case WanderCallKind::UNMOVED:
        return static_cast<int>(decide_unmoved_wander(policy, state, step.unmoved_blocked));
    }
    return -1;
}


const WanderRecoveryCase CASES[] = {
    {
        .name = "Pokemon::WanderRecovery - FRLG policy",
        .input = {
            .policy = {
                .max_waits = 4,
                .wait_duration = Milliseconds(1500),
                .probe_with_a = false,
            },
            .steps = {
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = false}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = false}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = false}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = false}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = false}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = false}},
                {.kind = WanderCallKind::MID_LEG, .reset_before = true, .mid_leg = {.has_wanderer_ahead = false, .can_resume_remaining = true}},
                {.kind = WanderCallKind::MID_LEG, .mid_leg = {.has_wanderer_ahead = true, .can_resume_remaining = true}},
                {.kind = WanderCallKind::MID_LEG, .mid_leg = {.has_wanderer_ahead = true, .can_resume_remaining = false}},
                {.kind = WanderCallKind::UNMOVED, .unmoved_blocked = false},
                {.kind = WanderCallKind::UNMOVED, .unmoved_blocked = true},
                {.kind = WanderCallKind::UNMOVED, .unmoved_blocked = true},
                {.kind = WanderCallKind::UNMOVED, .unmoved_blocked = true},
                {.kind = WanderCallKind::MID_LEG, .mid_leg = {.has_wanderer_ahead = true, .can_resume_remaining = true}},
            },
        },
        .expectation = {
            .outcomes = {
                {static_cast<int>(WallWanderDecision::NONE), 0},
                {static_cast<int>(WallWanderDecision::WAIT_AND_REHOLD), 1},
                {static_cast<int>(WallWanderDecision::WAIT_AND_REHOLD), 2},
                {static_cast<int>(WallWanderDecision::WAIT_AND_REHOLD), 3},
                {static_cast<int>(WallWanderDecision::WAIT_AND_REHOLD), 4},
                {static_cast<int>(WallWanderDecision::NONE), 4},
                {static_cast<int>(MidLegWanderDecision::NONE), 0},
                {static_cast<int>(MidLegWanderDecision::WAIT_AND_RESUME_REMAINING), 1},
                {static_cast<int>(MidLegWanderDecision::WAIT_AND_REANCHOR), 2},
                {static_cast<int>(UnmovedWanderDecision::NONE), 2},
                {static_cast<int>(UnmovedWanderDecision::WAIT_AND_RETRY), 3},
                {static_cast<int>(UnmovedWanderDecision::WAIT_AND_RETRY), 4},
                {static_cast<int>(UnmovedWanderDecision::NONE), 4},
                {static_cast<int>(MidLegWanderDecision::NONE), 4},
            },
        },
    },
    {
        .name = "Pokemon::WanderRecovery - BDSP policy",
        .input = {
            .policy = {
                .max_waits = 2,
                .wait_duration = Milliseconds(500),
                .probe_with_a = true,
            },
            .steps = {
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = false}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = true}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = true}},
                {.kind = WanderCallKind::WALL, .wall = {.touches_wander_zone = true, .probed_person = true}},
                {.kind = WanderCallKind::MID_LEG, .reset_before = true, .mid_leg = {.has_wanderer_ahead = true, .can_resume_remaining = true}},
                {.kind = WanderCallKind::UNMOVED, .unmoved_blocked = true},
                {.kind = WanderCallKind::UNMOVED, .unmoved_blocked = true},
            },
        },
        .expectation = {
            .outcomes = {
                {static_cast<int>(WallWanderDecision::NONE), 0},
                {static_cast<int>(WallWanderDecision::WAIT_AND_REHOLD), 1},
                {static_cast<int>(WallWanderDecision::WAIT_AND_REHOLD), 2},
                {static_cast<int>(WallWanderDecision::REPLAN_BEFORE_ZONE), 2},
                {static_cast<int>(MidLegWanderDecision::WAIT_AND_RESUME_REMAINING), 1},
                {static_cast<int>(UnmovedWanderDecision::WAIT_AND_RETRY), 2},
                {static_cast<int>(UnmovedWanderDecision::NONE), 2},
            },
        },
    },
};


class Test_WanderRecovery : public UnitTest{
public:
    explicit Test_WanderRecovery(const WanderRecoveryCase& test_case)
        : UnitTest(test_case.name)
        , m_case(test_case)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (m_case.input.steps.size() != m_case.expectation.outcomes.size()){
            return "Step count does not match expected outcome count.";
        }
        WanderWaitState state;
        for (size_t i = 0; i < m_case.input.steps.size(); i++){
            const int decision = run_step(m_case.input.policy, m_case.input.steps[i], state);
            const StepOutcome& expected = m_case.expectation.outcomes[i];
            if (decision != expected.decision || state.waits_used != expected.waits_used){
                return std::format("Step {} mismatch.", i);
            }
        }
        return true;
    }

private:
    const WanderRecoveryCase& m_case;
};


} // namespace


void add_tests_WanderRecovery(UnitTestDatabase& database){
    for (const auto& c : CASES){
        database.add<Test_WanderRecovery>(c);
    }
}


} // namespace Pokemon
} // namespace PokemonAutomation
