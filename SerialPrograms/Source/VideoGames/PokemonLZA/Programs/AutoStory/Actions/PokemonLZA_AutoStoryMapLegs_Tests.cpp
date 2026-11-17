/*  PokemonLZA AutoStory - Map legs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A leg to the 100 spot (432, 238) read by read: arriving, the walk's
 *  heading and hold, the corrections and where they stop, missing reads;
 *  and the waypoint of each of the patrol's catch spots.
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

//  The 100 spot's waypoint (ZONE1_SPOT_100).
constexpr MapPoint SPOT_100{.x = 432, .y = 238};

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
        .input = {.reads = {MapPoint{.x = 435, .y = 236}}},
        .expectation = {.moves = "arrived"},
    },
    {
        //  25 pixels east-north-east: a walk west-south-west, 3.7 s at 6.7
        //  pixels a second.
        .name = "a read off the waypoint walks toward it, then arrives",
        .input = {.reads = {MapPoint{.x = 456, .y = 231}, MapPoint{.x = 433, .y = 239}}},
        .expectation = {.moves = "walk 254 deg 3.7 s, arrived"},
    },
    {
        .name = "a read due south walks north",
        .input = {.reads = {MapPoint{.x = 432, .y = 258}}},
        .expectation = {.moves = "walk 0 deg 3.0 s"},
    },
    {
        .name = "a walk that ends short gets a correction",
        .input = {.reads = {MapPoint{.x = 456, .y = 231}, MapPoint{.x = 445, .y = 234}, MapPoint{.x = 431, .y = 238}}},
        .expectation = {.moves = "walk 254 deg 3.7 s, correction 1: walk 253 deg 2.0 s, arrived"},
    },
    {
        .name = "still off after the leg and two corrections: stops",
        .input = {.reads = {
            MapPoint{.x = 456, .y = 231}, MapPoint{.x = 420, .y = 220},
            MapPoint{.x = 445, .y = 250}, MapPoint{.x = 420, .y = 225},
        }},
        .expectation = {.moves = "walk 254 deg 3.7 s, correction 1: walk 146 deg 3.2 s, correction 2: walk 313 deg 2.6 s, off target"},
    },
    {
        .name = "a missing read gets a second look",
        .input = {.reads = {std::nullopt, MapPoint{.x = 433, .y = 238}}},
        .expectation = {.moves = "read again, arrived"},
    },
    {
        .name = "two missing reads in a row stop",
        .input = {.reads = {std::nullopt, std::nullopt}},
        .expectation = {.moves = "read again, pin not read"},
    },
    {
        .name = "a missing read after a walk is not a correction",
        .input = {.reads = {MapPoint{.x = 456, .y = 231}, std::nullopt, MapPoint{.x = 445, .y = 234}}},
        .expectation = {.moves = "walk 254 deg 3.7 s, read again, correction 1: walk 253 deg 2.0 s"},
    },
};

class Test_MapLeg : public UnitTest{
public:
    explicit Test_MapLeg(const LegCase& test)
        : UnitTest(std::format("PokemonLZA::AutoStory - Map legs: {}", test.name))
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        MapLeg leg(SPOT_100);
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
