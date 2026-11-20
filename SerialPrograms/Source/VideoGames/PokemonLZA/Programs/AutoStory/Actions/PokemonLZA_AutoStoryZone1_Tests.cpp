/*  PokemonLZA AutoStory - Wild Zone 1 Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's console-free walk plans in Wild Zone 1: the way out
 *  (zone_1_exit_legs()) on map pins; check_out_of_zone_1()
 *  on the engine's scripted backend (FakeStoryBackend), its map pin reads
 *  scripted; and legs to the zone's waypoints read by read (MapLeg):
 *  arriving, the walk's heading, corrections and where they stop, missing
 *  reads, and a catch spot's stop area.
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Testing/Pokemon_StoryFakeBackend.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryZone1.h"
#include "PokemonLZA_AutoStoryZone1_Tests.h"

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
        //  The point run to before the gate; none: one run at the gate.
        std::optional<MapPoint> waypoint;
    };
    Expectation expectation;
};

const ExitCase EXIT_CASES[] = {
    {
        //  console B, 2026-11-19 22:47:41: the run from here reached the A prompt.
        .name = "on the gate's line: one straight run",
        .input = {.start = {.x = 379, .y = 250}},
        .expectation = {.waypoint = std::nullopt},
    },
    {
        //  console B, 2026-11-19 23:40: 03.08b's start after the last catch;
        //  the run to (386, 252) first went 11 east, then back west past it.
        .name = "by the 60 spot, north of the gate's line: straight south onto the line",
        .input = {.start = {.x = 375, .y = 245}},
        .expectation = {.waypoint = MapPoint{.x = 375, .y = 252}},
    },
    {
        .name = "west of the gate: onto the line east of the gate",
        .input = {.start = {.x = 340, .y = 240}},
        .expectation = {.waypoint = MapPoint{.x = 386, .y = 252}},
    },
};

//  Runs coming in off the gate's line hit a pillar: the run at the gate
//  heads west along it whatever the start.
constexpr double GATE_HEADING = 270;
constexpr double GATE_HEADING_TOLERANCE = 10;

UnitTestResult check_exit(const ExitCase& test){
    const std::vector<MapPoint> legs = zone_1_exit_legs(test.input.start);
    const std::optional<MapPoint> waypoint = legs.size() == 2 ? std::optional<MapPoint>(legs.front()) : std::nullopt;
    const auto same = [](std::optional<MapPoint> a, std::optional<MapPoint> b){
        return a.has_value() == b.has_value() && (!a || (a->x == b->x && a->y == b->y));
    };
    if (legs.empty() || legs.size() > 2 || !same(waypoint, test.expectation.waypoint)){
        const auto shown = [](std::optional<MapPoint> point){
            return point ? std::format("({:.0f}, {:.0f}) first", point->x, point->y) : std::string("a straight run");
        };
        return std::format("Expected {}, got {} in {} legs.", shown(test.expectation.waypoint), shown(waypoint), legs.size());
    }
    const double heading = bearing(legs.size() == 1 ? test.input.start : legs[legs.size() - 2], legs.back());
    if (std::abs(heading - GATE_HEADING) > GATE_HEADING_TOLERANCE){
        return std::format("The run at the gate heads {:.0f}, not along its line ({:.0f}).", heading, GATE_HEADING);
    }
    return true;
}

class Test_ZoneExit : public UnitTest{
public:
    explicit Test_ZoneExit(const ExitCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - zone_1_exit_legs: {}", test.name))
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



//  Fixed waypoints for the leg logic, apart from the map data's.
constexpr MapWaypoint SPOT{.name = "the 100 spot", .pin = {.x = 432, .y = 238}, .stop_radius = CATCH_STOP_RADIUS};
constexpr MapWaypoint SPOT_60{.name = "the 60 spot", .pin = {.x = 386, .y = 252}, .stop_radius = CATCH_STOP_RADIUS};

std::string describe(const MapLegMove& move){
    switch (move.step){
    case MapLegStep::ARRIVED:
        return "arrived";
    case MapLegStep::WALK:
        return std::format(
            "{}walk {:.0f} deg",
            move.correction > 0 ? std::format("correction {}: ", move.correction) : std::string(),
            move.heading
        );
    case MapLegStep::READ_AGAIN:
        return "read again";
    case MapLegStep::OFF_TARGET:
        return "off target";
    case MapLegStep::PIN_NOT_READ:
        return "pin not read";
    }
    return "?";
}


struct LegCase{
    std::string name;
    struct Input{
        MapWaypoint waypoint;
        //  None: no pin on the map.
        std::vector<std::optional<MapPoint>> reads;
    };
    Input input;
    struct Expectation{
        //  ", "-joined.
        std::string moves;
    };
    Expectation expectation;
};

const LegCase LEG_CASES[] = {
    {
        .name = "a walk that ends short gets a correction, then arrives",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 456, .y = 231}, MapPoint{.x = 445, .y = 234}, MapPoint{.x = 431, .y = 238}}},
        .expectation = {.moves = "walk 254 deg, correction 1: walk 253 deg, arrived"},
    },
    {
        .name = "still off after the leg and two corrections: stops",
        .input = {.waypoint = SPOT, .reads = {
            MapPoint{.x = 456, .y = 231}, MapPoint{.x = 420, .y = 220},
            MapPoint{.x = 445, .y = 250}, MapPoint{.x = 420, .y = 225},
        }},
        .expectation = {.moves = "walk 254 deg, correction 1: walk 146 deg, correction 2: walk 313 deg, off target"},
    },
    {
        .name = "a missing read gets a second look",
        .input = {.waypoint = SPOT, .reads = {std::nullopt, MapPoint{.x = 433, .y = 238}}},
        .expectation = {.moves = "read again, arrived"},
    },
    {
        .name = "two missing reads in a row stop",
        .input = {.waypoint = SPOT, .reads = {std::nullopt, std::nullopt}},
        .expectation = {.moves = "read again, pin not read"},
    },
    {
        .name = "a missing read after a walk is not a correction",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 456, .y = 231}, std::nullopt, MapPoint{.x = 445, .y = 234}}},
        .expectation = {.moves = "walk 254 deg, read again, correction 1: walk 253 deg"},
    },
    {
        //  Console B, 2026-11-17 03.08: the leg to the 60 spot from
        //  (431, 236) ended at (380, 247), 7.8 pixels past it on its way;
        //  correction 1 turned back at heading 133, away from the Mareep.
        .name = "a walk past a catch spot on its way, within its stop area, arrives",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 380, .y = 247}}},
        .expectation = {.moves = "walk 250 deg, arrived"},
    },
    {
        .name = "a walk that ends off to the side of a catch spot, out of its stop area, gets a correction",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 384, .y = 265}}},
        .expectation = {.moves = "walk 250 deg, correction 1: walk 9 deg"},
    },
    {
        //  Console B, 2026-11-17 03.08b: zone_1_back_to_road started at
        //  the pin where 03.08's last catch ended and walked back to the
        //  60 spot before heading for the road.
        .name = "a first read within a catch spot's stop area arrives without a walk",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 380, .y = 247}}},
        .expectation = {.moves = "arrived"},
    },
};

class Test_MapLeg : public UnitTest{
public:
    explicit Test_MapLeg(const LegCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Map legs: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        MapLeg leg(m_test.input.waypoint.pin, m_test.input.waypoint.stop_radius);
        std::string moves;
        for (const std::optional<MapPoint>& read : m_test.input.reads){
            const MapLegMove move = leg.next(read);
            if (move.step == MapLegStep::WALK && move.hold <= Milliseconds(0)){
                return std::format("Expected a positive walk hold, got {} ms.", move.hold.count());
            }
            if (!moves.empty()){
                moves += ", ";
            }
            moves += describe(move);
        }
        if (moves != m_test.expectation.moves){
            return std::format("Expected \"{}\", got \"{}\".", m_test.expectation.moves, moves);
        }
        return true;
    }

private:
    const LegCase& m_test;
};

} // namespace


void add_tests_AutoStoryZone1(UnitTestDatabase& database){
    for (const ExitCase& test : EXIT_CASES){
        database.add<Test_ZoneExit>(test);
    }
    for (const OutOfZoneCase& test : OUT_OF_ZONE_CASES){
        database.add<Test_OutOfZone>(test);
    }
    for (const LegCase& test : LEG_CASES){
        database.add<Test_MapLeg>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
