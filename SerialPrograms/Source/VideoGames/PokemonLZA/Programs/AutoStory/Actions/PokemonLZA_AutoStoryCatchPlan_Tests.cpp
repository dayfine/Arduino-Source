/*  PokemonLZA AutoStory - Catch plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatchPlan_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

LockTarget plate(double x, double y, std::optional<bool> caught = false){
    LockTarget target;
    target.x = x;
    target.y = y;
    target.width = 0.05;
    target.caught = caught;
    return target;
}

std::string joined(const std::vector<int>& values){
    std::string out;
    for (const int value : values){
        out += out.empty() ? std::to_string(value) : ", " + std::to_string(value);
    }
    return out;
}

UnitTestResult test_nearest_uncaught(Logger&){
    //  The lowest plate wins, unless its species is caught; an unread mark
    //  counts as not caught.
    const std::vector<LockTarget> targets{
        plate(0.50, 0.25), plate(0.55, 0.45, true), plate(0.45, 0.35, std::nullopt), plate(0.60, 0.30),
    };
    const std::optional<size_t> best = nearest_uncaught(targets);
    if (best != 2){
        return std::format("Expected target 2, got {}.", best ? (int)*best : -1);
    }
    if (nearest_uncaught({plate(0.5, 0.4, true), plate(0.5, 0.5, true)})){
        return "Expected no target when all are caught.";
    }
    if (nearest_uncaught({})){
        return "Expected no target from no plates.";
    }
    return true;
}

UnitTestResult test_far_target(Logger&){
    if (!lock_target_far(plate(0.5, 0.20)) || lock_target_far(plate(0.5, 0.40))){
        return "Expected a plate high on the screen to be far, one lower to be near.";
    }
    return true;
}

UnitTestResult test_cycle_stops_at_edge(Logger&){
    //  Three targets, then a flick that finds no other: back left to the
    //  nearest, the middle one.
    LockTargetCycle cycle(4);
    const bool more = cycle.add(plate(0.50, 0.25)) && cycle.add(plate(0.52, 0.40)) && cycle.add(plate(0.48, 0.30));
    if (!more || cycle.add(plate(0.49, 0.31))){
        return "Expected the cycle to go on for three targets, then stop at the repeat.";
    }
    if (cycle.targets().size() != 3 || cycle.flicks_to(1) != -1){
        return std::format("Expected 3 targets and 1 flick left, got {} and {}.", cycle.targets().size(), cycle.flicks_to(1));
    }
    return true;
}

UnitTestResult test_cycle_wraps_round(Logger&){
    //  Two targets that go round: the lock is on the first again, so the
    //  second is one flick right.
    LockTargetCycle cycle(4);
    cycle.add(plate(0.50, 0.25));
    cycle.add(plate(0.50, 0.40));
    if (cycle.add(plate(0.51, 0.26))){
        return "Expected the cycle to stop when it comes round to the first target.";
    }
    if (cycle.targets().size() != 2 || cycle.flicks_to(1) != 1){
        return std::format("Expected 2 targets and 1 flick right, got {} and {}.", cycle.targets().size(), cycle.flicks_to(1));
    }
    return true;
}

UnitTestResult test_cycle_bounded(Logger&){
    LockTargetCycle cycle(4);
    size_t views = 0;
    for (int i = 0; i < 10; i++){
        views++;
        if (!cycle.add(plate(0.1 * i, 0.2 + 0.05 * i))){
            break;
        }
    }
    if (views != 4 || cycle.targets().size() != 4 || cycle.flicks_to(3) != 0){
        return std::format("Expected 4 views and the lock on the last, got {} views.", views);
    }
    return true;
}

UnitTestResult test_patrol_stops(Logger&){
    struct Row{
        int near;
        int far;
        int step;
        int laps;
        std::vector<int> expected;
    };
    const Row rows[] = {
        {60, 100, 20, 2, {60, 80, 100, 80, 60, 80, 100, 80, 60}},
        {60, 100, 20, 1, {60, 80, 100, 80, 60}},
        {60, 100, 30, 1, {60, 90, 100, 70, 60}},
        {60, 100, 20, 0, {60}},
        {60, 60, 20, 2, {60}},
    };
    for (const Row& row : rows){
        const std::vector<int> stops = patrol_stops(row.near, row.far, row.step, row.laps);
        if (stops != row.expected){
            return std::format("patrol_stops({}, {}, {}, {}): expected [{}], got [{}].", row.near, row.far, row.step, row.laps, joined(row.expected), joined(stops));
        }
    }
    return true;
}

struct CatchPlanInput{
    std::string name;
    UnitTestResult (*body)(Logger&);
};

struct CatchPlanExpectation{
};

struct CatchPlanCase{
    CatchPlanInput input;
    CatchPlanExpectation expectation;
};

const CatchPlanCase CATCH_PLAN_CASES[] = {
    {.input = {"Catches: the nearest target not caught", test_nearest_uncaught}, .expectation = {}},
    {.input = {"Catches: a high plate is a far target", test_far_target}, .expectation = {}},
    {.input = {"Catches: target cycle stops where a flick finds no other", test_cycle_stops_at_edge}, .expectation = {}},
    {.input = {"Catches: target cycle stops when it comes round", test_cycle_wraps_round}, .expectation = {}},
    {.input = {"Catches: target cycle is bounded", test_cycle_bounded}, .expectation = {}},
    {.input = {"Catches: patrol stops between 60 and 100", test_patrol_stops}, .expectation = {}},
};

class Test_AutoStoryCatchPlan : public UnitTest{
public:
    explicit Test_AutoStoryCatchPlan(const CatchPlanCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - {}", test_case.input.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return m_case.input.body(logger);
    }

private:
    const CatchPlanCase& m_case;
};

} // namespace


void add_tests_AutoStoryCatchPlan(UnitTestDatabase& database){
    for (const CatchPlanCase& item : CATCH_PLAN_CASES){
        database.add<Test_AutoStoryCatchPlan>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
