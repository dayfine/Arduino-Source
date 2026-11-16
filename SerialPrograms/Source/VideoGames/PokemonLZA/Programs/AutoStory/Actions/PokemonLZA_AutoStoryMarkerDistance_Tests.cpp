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
        //  The distance OCR read on each look, misreads included.
        std::vector<std::optional<int>> reads;
    };
    Input input;
    struct Expectation{
        //  The look the walk stops on (1-based); none: it walks on past
        //  every look.
        std::optional<size_t> stopped_on;
        std::optional<int> position;
    };
    Expectation expectation;
};

const StopWalkCase STOP_WALK_CASES[] = {
    {
        .name = "03.08's walk to 100 stops at 100 when the digits stop reading after 109",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            //  console B, 2026-11-16 07:17:10 to 07:17:32: the walk went on
            //  blind to a read of 37, out of Wild Zone 1 into the city.
            .reads = {
                120, 120, 120, NO_DIGITS, 5, 120, 120, NO_DIGITS, NO_DIGITS, NO_DIGITS, 109,
                NO_DIGITS, NO_DIGITS, 1, NO_DIGITS, NO_DIGITS, NO_DIGITS, 181, 21,
                NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS,
                NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS, 42, 39, 37,
            },
        },
        .expectation = {.stopped_on = 14, .position = 100},
    },
    {
        .name = "03.08's walk to 100 takes no lone 1 at its start",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            //  console B, 2026-11-16 07:33:56: the first look read "1" with
            //  the marker at about 135, and the walk stopped there at once.
            .reads = {1, 135, 132, 129, 126, 123, 120, 117, 114, 111, 108, 105, 102, 99},
        },
        .expectation = {.stopped_on = 14, .position = 99},
    },
    {
        .name = "03.08's walk to 100 takes no 1, 1, 4 at the rocks as a start",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            //  console B, 2026-11-16 07:41:06 to 07:41:08: the marker showed
            //  120 between Taunie's two rocks; "1", "f1F" and "{#4*" read 1,
            //  1 and 4, which agreed within 12 and ended the walk at once.
            .reads = {1, 1, 4, 120, 120, 117, 114, 111, 108, 105, 102, 99},
        },
        .expectation = {.stopped_on = 12, .position = 99},
    },
    {
        .name = "03.08's walk to 100 takes no run of 1s at its start",
        .input = {
            .direction = MarkerDirection::TOWARD,
            .stop_at = 100,
            //  console B, 2026-11-16 07:33:56: "1^" read 1 on the first
            //  looks with the marker at about 146.
            .reads = {1, 1, 1, 146, 143, 140, 137, 134, 131, 128, 125, 122, 119, 116, 113, 110, 107, 104, 101, 98},
        },
        .expectation = {.stopped_on = 20, .position = 98},
    },
    {
        .name = "a lone misread among a walk's first reads does not hold back the start",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 100, .reads = {130, 7, 124, 121, 118, 115, 112, 109, 106, 103, 100}},
        .expectation = {.stopped_on = 11, .position = 100},
    },
    {
        .name = "a walk back stops on the read at its stop",
        .input = {.direction = MarkerDirection::BACK, .stop_at = 100, .reads = {60, 66, 75, 84, 93, 101, 108}},
        .expectation = {.stopped_on = 6, .position = 101},
    },
    {
        .name = "a walk with no distance read walks on",
        .input = {.direction = MarkerDirection::TOWARD, .stop_at = 100, .reads = {NO_DIGITS, NO_DIGITS, NO_DIGITS, NO_DIGITS}},
        .expectation = {.stopped_on = std::nullopt, .position = std::nullopt},
    },
};

template <typename Type>
std::string text(const std::optional<Type>& value){
    return value ? std::to_string(*value) : "none";
}

//  The walk's loop: a look's read, the stop checked, else a step on.
UnitTestResult check_stop_walk(const StopWalkCase& test){
    MarkerDistance distance(test.input.direction);
    std::optional<size_t> stopped_on;
    for (size_t look = 0; look < test.input.reads.size(); look++){
        const std::optional<int> read = test.input.reads[look];
        if (read){
            distance.read(*read);
        }
        if (distance.reached(test.input.stop_at)){
            stopped_on = look + 1;
            break;
        }
        distance.stepped();
    }
    const StopWalkCase::Expectation& expected = test.expectation;
    if (stopped_on != expected.stopped_on || distance.position() != expected.position){
        return std::format(
            "expected stopped on look {} at {}, got look {} at {}",
            text(expected.stopped_on), text(expected.position), text(stopped_on), text(distance.position())
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
