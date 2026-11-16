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
