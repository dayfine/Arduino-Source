/*  PokemonLZA AutoStory - Wild Zone 1 map Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  zone_1_exit_via() on map pins, and check_out_of_zone_1() on the
 *  engine's scripted backend (FakeStoryBackend), its map pin reads scripted.
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryZone1Map.h"
#include "PokemonLZA_AutoStoryZone1Map_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



namespace{

using FakeBackend = Pokemon::FakeStoryBackend;

//  Reads `pins` in turn, the last repeating, counting into `reads`; none
//  when `pins` is empty.
MoveOnlyFunction<std::optional<MapPoint>()> pins_of(const std::vector<MapPoint>& pins, size_t& reads){
    if (pins.empty()){
        return nullptr;
    }
    return [pins, &reads]() -> std::optional<MapPoint>{
        const MapPoint pin = pins[std::min(reads, pins.size() - 1)];
        reads++;
        return pin;
    };
}

//  The map pin on the street outside the far gate (03.08b's menu save,
//  console B 2026-11-17 01:06), and at Wild Zone 1's 60 catch spot.
constexpr MapPoint OUT_OF_ZONE_PIN{.x = 401, .y = 225};
constexpr MapPoint IN_ZONE_PIN{.x = 386, .y = 252};
//  Outside the zone's outline but not where the map centres a player out
//  of every zone.
constexpr MapPoint OFF_CENTRE_OUT_PIN{.x = 420, .y = 262};


struct ExitCase{
    std::string name;
    struct Input{
        MapPoint start;
    };
    Input input;
    struct Expectation{
        //  One run at the gate, no waypoint first.
        bool straight = false;
    };
    Expectation expectation;
};

const ExitCase EXIT_CASES[] = {
    {
        //  console B, 2026-11-19 22:47:41: the run from here reached the A prompt.
        .name = "on the gate's line: one straight run",
        .input = {.start = {.x = 379, .y = 250}},
        .expectation = {.straight = true},
    },
    {
        //  console B, 2026-11-19 22:47:04: 03.08b's start after the last catch.
        .name = "by the 60 spot, north of the gate's line: onto the line first",
        .input = {.start = {.x = 377, .y = 246}},
        .expectation = {.straight = false},
    },
};

//  Runs coming in off the gate's line hit a pillar: the run at the gate
//  heads west along it whatever the start.
constexpr double GATE_HEADING = 270;
constexpr double GATE_HEADING_TOLERANCE = 10;

UnitTestResult check_exit(const ExitCase& test){
    const std::optional<MapPoint> via = zone_1_exit_via(test.input.start);
    if (via.has_value() == test.expectation.straight){
        return std::format("Expected {}, got {}.", test.expectation.straight ? "a straight run" : "a waypoint first", via ? "a waypoint" : "a straight run");
    }
    const double heading = bearing(via.value_or(test.input.start), ZONE1_FAR_GATE.pin);
    if (std::abs(heading - GATE_HEADING) > GATE_HEADING_TOLERANCE){
        return std::format("The run at the gate heads {:.0f}, not along its line ({:.0f}).", heading, GATE_HEADING);
    }
    return true;
}

class Test_ZoneExit : public UnitTest{
public:
    explicit Test_ZoneExit(const ExitCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - zone_1_exit_via: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_exit(m_test);
    }

private:
    const ExitCase& m_test;
};


struct OutOfZoneCase{
    std::string name;
    struct Input{
        //  The last repeating; none: no reader.
        std::vector<MapPoint> pins;
    };
    Input input;
    struct Expectation{
        //  None: out of the zone.
        std::optional<std::string> failure;
        //  Map openings.
        size_t reads = 0;
    };
    Expectation expectation;
};

const OutOfZoneCase OUT_OF_ZONE_CASES[] = {
    {
        //  console B, 2026-11-19 22:47:51: two map openings for one answer.
        .name = "out at the map's centre: one map read",
        .input = {.pins = {OUT_OF_ZONE_PIN}},
        .expectation = {.failure = std::nullopt, .reads = 1},
    },
    {
        //  console B, 2026-11-18 13:37: 03.08b saved in Wild Zone 1.
        .name = "in the zone: no save there",
        .input = {.pins = {IN_ZONE_PIN}},
        .expectation = {.failure = "in_wild_zone", .reads = 1},
    },
    {
        .name = "out off the map's centre, in on the next read: in the zone",
        .input = {.pins = {OFF_CENTRE_OUT_PIN, IN_ZONE_PIN}},
        .expectation = {.failure = "in_wild_zone", .reads = 2},
    },
    {
        .name = "no map pin reader: not known to be out",
        .input = {.pins = {}},
        .expectation = {.failure = "zone_unread", .reads = 0},
    },
};

UnitTestResult check_out_of_zone(const OutOfZoneCase& test, CancellableScope& scope){
    FakeBackend backend(Pokemon::FakeStoryScript{.looks = {}}, scope);
    size_t reads = 0;
    const std::optional<Pokemon::StoryFailure> outcome = check_out_of_zone_1(backend, pins_of(test.input.pins, reads));
    const std::optional<std::string> failure = !outcome ? std::nullopt : std::optional<std::string>(outcome->code);
    if (failure != test.expectation.failure){
        return std::format("Expected {}, got {}.", test.expectation.failure.value_or("a pass"), (outcome ? std::format("{}", *outcome) : std::string("ok")));
    }
    if (reads != test.expectation.reads){
        return std::format("Expected {} map reads, got {}.", test.expectation.reads, reads);
    }
    return true;
}

class Test_OutOfZone : public UnitTest{
public:
    explicit Test_OutOfZone(const OutOfZoneCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - check_out_of_zone_1: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope& scope) const override{
        return check_out_of_zone(m_test, scope);
    }

private:
    const OutOfZoneCase& m_test;
};

} // namespace


void add_tests_AutoStoryZone1Map(UnitTestDatabase& database){
    for (const ExitCase& test : EXIT_CASES){
        database.add<Test_ZoneExit>(test);
    }
    for (const OutOfZoneCase& test : OUT_OF_ZONE_CASES){
        database.add<Test_OutOfZone>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
