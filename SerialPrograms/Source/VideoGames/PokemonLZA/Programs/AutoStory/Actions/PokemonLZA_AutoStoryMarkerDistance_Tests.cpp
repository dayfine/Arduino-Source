/*  PokemonLZA AutoStory - Marker distance Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryMarkerDistance.h"
#include "PokemonLZA_AutoStoryMarkerDistance_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  No digits on a look: the marker shows, its distance does not read.
constexpr std::optional<int> NO_DIGITS = std::nullopt;


struct StopWalkCase{
    std::string name;
    struct Input{
        MarkerDirection direction = MarkerDirection::TOWARD;
        int stop_at = 0;
        std::optional<int> floor;
        //  The distance OCR read on each look, misreads included.
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        //  The look the walk arrives on (1-based); none: it never does.
        std::optional<size_t> arrived_on;
        std::optional<int> position;
        //  The steps on, toward or back, before it arrived, was blocked or
        //  ran out of looks.
        int steps = 0;
        //  The new views tried with no read taken (nudges, sidesteps), and
        //  BLOCKED last when the walk ends on it.
        std::vector<MarkerMove> views;
    };
    Expectation expectation;
};

//  Wild Zone 1's floor: 3 short of the stop at 60, just before the far gate.
constexpr int ZONE_FLOOR = 57;

constexpr MarkerMove NUDGE_LEFT = MarkerMove::NUDGE_CAMERA_LEFT;
constexpr MarkerMove NUDGE_RIGHT = MarkerMove::NUDGE_CAMERA_RIGHT;
constexpr MarkerMove SIDESTEP_LEFT = MarkerMove::SIDESTEP_LEFT;
constexpr MarkerMove SIDESTEP_RIGHT = MarkerMove::SIDESTEP_RIGHT;
constexpr MarkerMove BLOCKED = MarkerMove::BLOCKED;

const StopWalkCase STOP_WALK_CASES[] = {
    {
        .name = "03.08's walk to 100 stands, tries new views and ends blocked when the digits stop reading after 109",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            .floor = ZONE_FLOOR,
            //  console B, 2026-11-16 07:17:10 to 07:17:32: the walk stepped
            //  on through the unread and garbage looks to a read of 37, out
            //  of Wild Zone 1 through the far gate into the city.
            .reads = {
                120, 120, 120, NO_DIGITS, 5, 120, 120, NO_DIGITS, NO_DIGITS, NO_DIGITS, 109,
                NO_DIGITS, NO_DIGITS, 1, NO_DIGITS, NO_DIGITS, NO_DIGITS, 181, 21,
                NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS,
                NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, 42, 39, 37,
            },
        },
        .expectation = {
            .arrived_on = std::nullopt, .position = 109, .steps = 4,
            .views = {NUDGE_LEFT, NUDGE_LEFT, NUDGE_RIGHT, SIDESTEP_LEFT, SIDESTEP_RIGHT, SIDESTEP_LEFT, SIDESTEP_RIGHT, BLOCKED},
        },
    },
    {
        .name = "03.08's walk to 100 goes on to 100 once the digits read again after the garbage",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            .floor = ZONE_FLOOR,
            .reads = {120, 120, 120, 109, NO_DIGITS, 1, 181, 21, NO_DIGITS, 109, 106, 103, 100},
        },
        .expectation = {.arrived_on = 13, .position = 100, .steps = 5, .views = {NUDGE_LEFT}},
    },
    {
        .name = "03.08's walk to 100 takes no lone 1 at its start",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            .floor = ZONE_FLOOR,
            //  console B, 2026-11-16 07:33:56: the first look read "1" with
            //  the marker at about 135, and the walk stopped there at once.
            .reads = {1, 135, 135, 135, 132, 129, 126, 123, 120, 117, 114, 111, 108, 105, 102, 99},
        },
        .expectation = {.arrived_on = 16, .position = 99, .steps = 12, .views = {NUDGE_LEFT}},
    },
    {
        .name = "03.08's walk to 100 takes no 1, 1, 4 at the rocks as a start",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            .floor = ZONE_FLOOR,
            //  console B, 2026-11-16 07:41:06 to 07:41:08: the marker showed
            //  120 between Taunie's two rocks; "1", "f1F" and "{#4*" read 1,
            //  1 and 4, and the walk ended at once.
            .reads = {1, 1, 4, 120, 120, 120, 117, 114, 111, 108, 105, 102, 99},
        },
        .expectation = {.arrived_on = 13, .position = 99, .steps = 7, .views = {NUDGE_LEFT}},
    },
    {
        .name = "03.08's start reads of 1, 1 and 4 at the rocks give no position",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 100, .floor = ZONE_FLOOR, .reads = {1, 1, 4, 1, 4}},
        .expectation = {.arrived_on = std::nullopt, .position = std::nullopt, .steps = 0, .views = {NUDGE_LEFT}},
    },
    {
        .name = "a walk toward stops at its floor on reads that agree below the stop",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 40, .floor = ZONE_FLOOR, .reads = {70, 70, 70, 64, 60, 57, 54}},
        .expectation = {.arrived_on = 6, .position = 57, .steps = 3},
    },
    {
        .name = "a lone misread among a walk's first reads does not hold back the start",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 100, .floor = ZONE_FLOOR, .reads = {130, 7, 130, 130, 127, 124, 121, 118, 115, 112, 109, 106, 103, 100}},
        .expectation = {.arrived_on = 14, .position = 100, .steps = 10, .views = {NUDGE_LEFT}},
    },
    {
        .name = "a walk back stops on the read at its stop",
        .input = {.direction = MarkerDirection::BACK, .stop_at = 100, .floor = std::nullopt, .reads = {49, 49, 49, 55, 62, 70, 79, 88, 96, 101}},
        .expectation = {.arrived_on = 10, .position = 101, .steps = 7},
    },
    {
        .name = "03.08's walk to 100 nudges the camera, then sidesteps, when a speech bubble hides the digits",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            .floor = ZONE_FLOOR,
            //  run 4, console B, 2026-11-16 12:44 to 12:46: taken at 120 and
            //  114, then an NPC's bubble over the marker read nothing, 1, 4,
            //  5, 7, 14 and 22; a step to the side clears it.
            .reads = {
                120, 120, 120, 117, 114,
                NO_DIGITS, 1, 4, 5, 7, 14, 22, NO_DIGITS, 4,
                111, 108, 105, 102, 99,
            },
        },
        .expectation = {.arrived_on = 19, .position = 99, .steps = 7, .views = {NUDGE_LEFT, NUDGE_RIGHT, SIDESTEP_LEFT}},
    },
    {
        .name = "a walk whose digits stay hidden ends after two nudges and four sidesteps",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            .floor = ZONE_FLOOR,
            .reads = {
                120, 120, 120, 114,
                22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22,
                111, 108,
            },
        },
        .expectation = {
            .arrived_on = std::nullopt, .position = 114, .steps = 2,
            .views = {NUDGE_LEFT, NUDGE_RIGHT, SIDESTEP_LEFT, SIDESTEP_RIGHT, SIDESTEP_LEFT, SIDESTEP_RIGHT, BLOCKED},
        },
    },
    {
        .name = "a walk with no distance read never steps",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 100, .floor = ZONE_FLOOR, .reads = {NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS}},
        .expectation = {.arrived_on = std::nullopt, .position = std::nullopt, .steps = 0, .views = {NUDGE_LEFT}},
    },
};

template <typename Type>
std::string text(const std::optional<Type>& value){
    return value ? std::to_string(*value) : "none";
}

std::string text(const std::vector<MarkerMove>& views){
    std::string out;
    for (MarkerMove view : views){
        out += std::format("{}{}", out.empty() ? "" : ",", (int)view);
    }
    return std::format("{{{}}}", out);
}

//  The walk's loop: each look's read decides a step, a re-read, a new view
//  or the end.
UnitTestResult check_stop_walk(const StopWalkCase& test){
    MarkerDistance distance(test.input.direction, test.input.floor);
    std::optional<size_t> arrived_on;
    int steps = 0;
    std::vector<MarkerMove> views;
    for (size_t look = 0; look < test.input.reads.size(); look++){
        const MarkerMove move = distance.next(test.input.reads[look], test.input.stop_at);
        if (move == MarkerMove::ARRIVED){
            arrived_on = look + 1;
            break;
        }
        if (move == MarkerMove::STEP){
            steps++;
        }else if (move != MarkerMove::RECENTER && move != MarkerMove::REREAD){
            views.push_back(move);
        }
        if (move == MarkerMove::BLOCKED){
            break;
        }
    }
    const StopWalkCase::Expectation& expected = test.expectation;
    if (arrived_on != expected.arrived_on || distance.position() != expected.position || steps != expected.steps || views != expected.views){
        return std::format(
            "expected arrived on look {} at {} after {} steps and views {}, got look {} at {} after {} steps and views {}",
            text(expected.arrived_on), text(expected.position), expected.steps, text(expected.views),
            text(arrived_on), text(distance.position()), steps, text(views)
        );
    }
    return true;
}


class StopWalkTest : public UnitTest{
public:
    explicit StopWalkTest(StopWalkCase test_case)
        : UnitTest(std::format("PokemonLZA::AutoStory - MarkerDistance: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_stop_walk(m_case);
    }
private:
    StopWalkCase m_case;
};

} // namespace


void add_tests_AutoStoryMarkerDistance(UnitTestDatabase& database){
    for (const StopWalkCase& test : STOP_WALK_CASES){
        database.add<StopWalkTest>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
