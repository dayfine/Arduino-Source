/*  PokemonLZA AutoStory - Map legs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Legs to Wild Zone 1's waypoints read by read: arriving, the walk's
 *  heading and hold, the corrections and where they stop, missing reads,
 *  a catch spot's stop area; and the waypoint of each of the patrol's
 *  catch spots.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonLZA_AutoStoryMapLegs.h"
#include "PokemonLZA_AutoStoryZone1Map.h"
#include "PokemonLZA_AutoStoryZone1Road.h"
#include "PokemonLZA_AutoStoryMapLegs_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Fixed waypoints for the leg logic, apart from the map data's.
constexpr MapWaypoint SPOT{.name = "the 100 spot", .pin = {.x = 432, .y = 238}, .stop_radius = CATCH_STOP_RADIUS};
constexpr MapWaypoint ROAD{.name = "the road entry", .pin = {.x = 456, .y = 231}};
constexpr MapWaypoint SPOT_60{.name = "the 60 spot", .pin = {.x = 386, .y = 252}, .stop_radius = CATCH_STOP_RADIUS};

std::string describe(const MapLegMove& move){
    switch (move.step){
    case MapLegStep::ARRIVED:
        return "arrived";
    case MapLegStep::WALK:
        return std::format(
            "{}walk {:.0f} deg {:.1f} s",
            move.correction > 0 ? std::format("correction {}: ", move.correction) : std::string(),
            move.heading, (double)move.hold.count() / 1000
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
        //  The pin reads in turn; none: no pin on the map.
        std::vector<std::optional<MapPoint>> reads;
    };
    Input input;
    struct Expectation{
        //  The moves after each read, ", "-joined.
        std::string moves;
    };
    Expectation expectation;
};

const LegCase LEG_CASES[] = {
    {
        .name = "a read within the tolerance arrives",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 435, .y = 236}}},
        .expectation = {.moves = "arrived"},
    },
    {
        //  25 pixels east-north-east: a walk west-south-west, 3.6 s at 7.0
        //  pixels a second.
        .name = "a read off the waypoint walks toward it, then arrives",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 456, .y = 231}, MapPoint{.x = 433, .y = 239}}},
        .expectation = {.moves = "walk 254 deg 3.6 s, arrived"},
    },
    {
        .name = "a read due south walks north",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 432, .y = 258}}},
        .expectation = {.moves = "walk 0 deg 2.9 s"},
    },
    {
        .name = "a walk that ends short gets a correction",
        .input = {.waypoint = SPOT, .reads = {MapPoint{.x = 456, .y = 231}, MapPoint{.x = 445, .y = 234}, MapPoint{.x = 431, .y = 238}}},
        .expectation = {.moves = "walk 254 deg 3.6 s, correction 1: walk 253 deg 1.9 s, arrived"},
    },
    {
        .name = "still off after the leg and two corrections: stops",
        .input = {.waypoint = SPOT, .reads = {
            MapPoint{.x = 456, .y = 231}, MapPoint{.x = 420, .y = 220},
            MapPoint{.x = 445, .y = 250}, MapPoint{.x = 420, .y = 225},
        }},
        .expectation = {.moves = "walk 254 deg 3.6 s, correction 1: walk 146 deg 3.1 s, correction 2: walk 313 deg 2.5 s, off target"},
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
        .expectation = {.moves = "walk 254 deg 3.6 s, read again, correction 1: walk 253 deg 1.9 s"},
    },
    {
        //  Console B, 2026-11-17 03.08: the leg to the 60 spot from
        //  (431, 236) ended at (380, 247), 7.8 pixels past it on its way;
        //  correction 1 turned back at heading 133, away from the Mareep.
        .name = "a walk past a catch spot on its way, within its stop area, arrives",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 380, .y = 247}}},
        .expectation = {.moves = "walk 250 deg 6.8 s, arrived"},
    },
    {
        .name = "a walk that ends off to the side of a catch spot, out of its stop area, gets a correction",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 384, .y = 265}}},
        .expectation = {.moves = "walk 250 deg 6.8 s, correction 1: walk 9 deg 1.9 s"},
    },
    {
        .name = "a walk that ends short of a catch spot gets a correction forward",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 431, .y = 236}, MapPoint{.x = 394, .y = 249}}},
        .expectation = {.moves = "walk 250 deg 6.8 s, correction 1: walk 249 deg 1.2 s"},
    },
    {
        //  Console B, 2026-11-17 03.08b: zone_1_back_to_road started at
        //  the pin where 03.08's last catch ended and walked back to the
        //  60 spot before heading for the road.
        .name = "a first read within a catch spot's stop area arrives without a walk",
        .input = {.waypoint = SPOT_60, .reads = {MapPoint{.x = 380, .y = 247}}},
        .expectation = {.moves = "arrived"},
    },
    {
        .name = "a walk past the road entry gets a correction back",
        .input = {.waypoint = ROAD, .reads = {MapPoint{.x = 464, .y = 229}, MapPoint{.x = 448, .y = 233}}},
        .expectation = {.moves = "walk 256 deg 1.2 s, correction 1: walk 76 deg 1.2 s"},
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
            moves += (moves.empty() ? "" : ", ") + describe(leg.next(read));
        }
        if (moves != m_test.expectation.moves){
            return std::format("Expected \"{}\", got \"{}\".", m_test.expectation.moves, moves);
        }
        return true;
    }

private:
    const LegCase& m_test;
};

struct WaypointCase{
    std::string name;
    struct Input{
        int spot;
    };
    Input input;
    struct Expectation{
        //  The waypoint's name; none: no waypoint.
        std::optional<std::string_view> waypoint;
    };
    Expectation expectation;
};

const WaypointCase WAYPOINT_CASES[] = {
    {.name = "the first catch spot", .input = {.spot = CATCH_SPOTS[0]}, .expectation = {.waypoint = "the 100 spot"}},
    {.name = "the second catch spot", .input = {.spot = CATCH_SPOTS[1]}, .expectation = {.waypoint = "the 60 spot"}},
    {.name = "a distance that is no catch spot", .input = {.spot = 87}, .expectation = {.waypoint = std::nullopt}},
};

class Test_CatchSpotWaypoint : public UnitTest{
public:
    explicit Test_CatchSpotWaypoint(const WaypointCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Map legs: catch spot waypoint: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::optional<MapWaypoint> waypoint = catch_spot_waypoint(m_test.input.spot);
        const std::optional<std::string_view> name = waypoint ? std::optional<std::string_view>(waypoint->name) : std::nullopt;
        if (name != m_test.expectation.waypoint){
            return std::format(
                "Expected {}, got {}.",
                m_test.expectation.waypoint.value_or("no waypoint"), name.value_or("no waypoint")
            );
        }
        return true;
    }

private:
    const WaypointCase& m_test;
};

} // namespace


void add_tests_AutoStoryMapLegs(UnitTestDatabase& database){
    for (const LegCase& test : LEG_CASES){
        database.add<Test_MapLeg>(test);
    }
    for (const WaypointCase& test : WAYPOINT_CASES){
        database.add<Test_CatchSpotWaypoint>(test);
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
