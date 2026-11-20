/*  PokemonLZA AutoStory - Catch plan Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each kind of catch-plan choice is a table of cases, each a name, the
 *  plates or patrol bounds it is given, and what it should choose.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryCatchPlan.h"
#include "PokemonLZA_AutoStoryCatchPlan_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  `caught` unset: the mark wasn't read.
struct Plate{
    double x;
    double y;
    std::optional<bool> caught = false;
};

LockTarget lock_target(const Plate& plate){
    LockTarget target;
    target.x = plate.x;
    target.y = plate.y;
    target.caught = plate.caught;
    return target;
}


struct SettleCase{
    std::string name;
    struct Input{
        //  One per look; the last repeats.
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        std::optional<int> slots;
    };
    Expectation expectation;
};

const SettleCase SETTLE_CASES[] = {
    {
        //  03.08 read 4 of 5 and failed; its retry at once read 5 (console B, 2026-11-18).
        .name = "the last catch joining the bar a second after the first read counts",
        .input = {.reads = {4, 4, 4, 4, 5}},
        .expectation = {.slots = 5},
    },
};

class Test_SettledPartySlots : public UnitTest{
public:
    explicit Test_SettledPartySlots(const SettleCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: settled party bar: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::optional<int>>& reads = m_case.input.reads;
        size_t look = 0;
        const std::optional<int> got = settled_party_slots([&]{
            return reads[std::min(look++, reads.size() - 1)];
        });
        if (got != m_case.expectation.slots){
            return std::format("Expected {}, got {}.", m_case.expectation.slots.value_or(-1), got.value_or(-1));
        }
        return true;
    }

private:
    const SettleCase& m_case;
};


struct CycleCase{
    std::string name;
    struct Input{
        size_t max_views;
        //  The target locked on after each flick, the first before any.
        std::vector<Plate> views;
        size_t flick_target;
    };
    Input input;
    struct Expectation{
        //  Up to and with the one where the cycle stops.
        size_t views_added;
        size_t targets;
        //  Left is negative.
        int flicks;
    };
    Expectation expectation;
};

const CycleCase CYCLE_CASES[] = {
    {
        .name = "stops where a flick finds no other, the nearest back left",
        .input = {.max_views = 4, .views = {{0.50, 0.25}, {0.52, 0.40}, {0.48, 0.30}, {0.49, 0.31}}, .flick_target = 1},
        .expectation = {.views_added = 4, .targets = 3, .flicks = -1},
    },
    {
        .name = "stops when it comes round to the first, the second a flick right",
        .input = {.max_views = 4, .views = {{0.50, 0.25}, {0.50, 0.40}, {0.51, 0.26}}, .flick_target = 1},
        .expectation = {.views_added = 3, .targets = 2, .flicks = 1},
    },
    {
        .name = "is bounded by its views, the lock on the last",
        .input = {
            .max_views = 4,
            .views = {{0.0, 0.20}, {0.1, 0.25}, {0.2, 0.30}, {0.3, 0.35}, {0.4, 0.40}, {0.5, 0.45}, {0.6, 0.50}},
            .flick_target = 3,
        },
        .expectation = {.views_added = 4, .targets = 4, .flicks = 0},
    },
};

class Test_LockTargetCycle : public UnitTest{
public:
    explicit Test_LockTargetCycle(const CycleCase& test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - Catches: target cycle: {}", test_case.name))
        , m_case(test_case)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LockTargetCycle cycle(m_case.input.max_views);
        size_t added = 0;
        for (const Plate& view : m_case.input.views){
            added++;
            if (!cycle.add(lock_target(view))){
                break;
            }
        }
        const size_t targets = cycle.targets().size();
        const int flicks = cycle.flicks_to(m_case.input.flick_target);
        const CycleCase::Expectation& expected = m_case.expectation;
        if (added != expected.views_added || targets != expected.targets || flicks != expected.flicks){
            return std::format(
                "Expected {} views added, {} targets and {} flicks; got {}, {} and {}.",
                expected.views_added, expected.targets, expected.flicks, added, targets, flicks
            );
        }
        return true;
    }

private:
    const CycleCase& m_case;
};


} // namespace


void add_tests_AutoStoryCatchPlan(UnitTestDatabase& database){
    for (const SettleCase& item : SETTLE_CASES){
        database.add<Test_SettledPartySlots>(item);
    }
    for (const CycleCase& item : CYCLE_CASES){
        database.add<Test_LockTargetCycle>(item);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
