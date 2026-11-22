/*  Door Approach Schedule Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon_DoorApproach.h"
#include "Pokemon_DoorApproach_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;

constexpr DoorApproachTiming BDSP_WALK_TIMING{
    .start_delay = 135ms,
    .per_tile = 275ms,
    .drive_margin = 500ms,
    .fade_margin = 800ms,
    .min_fade_wait = 4500ms,
    .fallback_timeout = 12000ms,
    .dark_from_share = 0.85,
};

constexpr DoorApproachTiming BDSP_RUN_TIMING{
    .start_delay = 135ms,
    .per_tile = 175ms,
    .drive_margin = 500ms,
    .fade_margin = 800ms,
    .min_fade_wait = 4500ms,
    .fallback_timeout = 12000ms,
    .dark_from_share = 0.85,
};

struct DoorApproachCase{
    struct Input{
        std::vector<size_t> distances = {};
        DoorApproachTiming timing = {};
    };
    struct Expectation{
        DoorApproachPlan plan = {};
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_DoorApproach : public UnitTest{
public:
    explicit Test_DoorApproach(DoorApproachCase test_case)
        : UnitTest("Pokemon::DoorApproach - " + test_case.name)
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const DoorApproachPlan got = plan_door_approach(m_case.input.distances, m_case.input.timing);
        const DoorApproachPlan& want = m_case.expectation.plan;
        if (got != want){
            return std::format(
                "expected drive={}ms timeout={}ms dark_from={}ms, got drive={}ms timeout={}ms dark_from={}ms",
                want.drive_hold.count(), want.fade_timeout.count(), want.dark_from.count(),
                got.drive_hold.count(), got.fade_timeout.count(), got.dark_from.count()
            );
        }
        return true;
    }

private:
    DoorApproachCase m_case;
};

const DoorApproachCase DOOR_APPROACH_CASES[] = {
    {
        .name = "empty distances fall back to full timeout",
        .input = {.distances = {}, .timing = BDSP_WALK_TIMING},
        .expectation = {.plan = {.drive_hold = 12000ms, .fade_timeout = 12000ms, .dark_from = 0ms}},
    },
    {
        .name = "all-zero distances fall back to full timeout",
        .input = {.distances = {0, 0}, .timing = BDSP_WALK_TIMING},
        .expectation = {.plan = {.drive_hold = 12000ms, .fade_timeout = 12000ms, .dark_from = 0ms}},
    },
    {
        .name = "single-tile door releases early while watching full fade window",
        .input = {.distances = {1}, .timing = BDSP_WALK_TIMING},
        .expectation = {.plan = {.drive_hold = 910ms, .fade_timeout = 4500ms, .dark_from = 348ms}},
    },
    {
        .name = "multi-tile pose uses max distance for drive and min for dark_from (RivalHouse2F stairs)",
        .input = {.distances = {1, 2, 3}, .timing = BDSP_WALK_TIMING},
        .expectation = {.plan = {.drive_hold = 1460ms, .fade_timeout = 4500ms, .dark_from = 348ms}},
    },
    {
        .name = "zero entries in multi-tile pose are ignored",
        .input = {.distances = {0, 3, 1, 0}, .timing = BDSP_WALK_TIMING},
        .expectation = {.plan = {.drive_hold = 1460ms, .fade_timeout = 4500ms, .dark_from = 348ms}},
    },
    {
        .name = "long hall run extends fade_timeout past min_fade_wait",
        .input = {.distances = {25}, .timing = BDSP_RUN_TIMING},
        .expectation = {.plan = {.drive_hold = 5010ms, .fade_timeout = 5310ms, .dark_from = 3833ms}},
    },
    {
        .name = "drive_hold never exceeds fade_timeout when drive_margin exceeds fade_margin",
        .input = {
            .distances = {25},
            .timing = {
                .start_delay = 100ms,
                .per_tile = 200ms,
                .drive_margin = 900ms,
                .fade_margin = 400ms,
                .min_fade_wait = 4500ms,
                .fallback_timeout = 12000ms,
                .dark_from_share = 0.8,
            },
        },
        .expectation = {.plan = {.drive_hold = 6000ms, .fade_timeout = 6000ms, .dark_from = 4080ms}},
    },
};

} // namespace

void add_tests_DoorApproach(UnitTestDatabase& database){
    for (const DoorApproachCase& item : DOOR_APPROACH_CASES){
        database.add<Test_DoorApproach>(item);
    }
}


} // namespace Pokemon
} // namespace PokemonAutomation
