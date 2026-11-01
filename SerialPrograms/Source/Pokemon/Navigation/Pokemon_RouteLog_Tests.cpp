/*  Route Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The route log lines, word for word: BDSP-style position sets (several
 *  tiles, map names with spaces) and FRLG-style single tiles.
 *
 */

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_Navigation_Tests.h"
#include "Pokemon_RouteLog.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

class Test_RouteLog : public UnitTest{
public:
    Test_RouteLog(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("Pokemon::RouteLog - " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
};

//  A BDSP-style position: every tile the player may be on.
const std::vector<RouteTile> BDSP_FROM{{"Route 202", {15, 25}}, {"Route 202", {16, 25}}};
const std::vector<RouteTile> BDSP_WALL{{"Route 202", {23, 25}}, {"Route 202", {30, 26}}};
//  An FRLG-style position: the one tile.
const std::vector<RouteTile> FRLG_TILE{{"Route1", {12, 21}}};
const std::vector<RouteTile> BDSP_GRASS{{"Route 202", {23, 17}}};
const std::vector<RouteTile> FRLG_NORTH{{"Route1", {12, 13}}};
const std::vector<RouteTile> TRAINER_SEEN{{"Route 202", {20, 10}}};
const std::vector<RouteTile> ARRIVED{{"Route 202", {20, 25}}};

const RouteLogLeg EAST_TO_WALL{.action = RouteLegAction::WALK, .direction = Direction::EAST, .tiles = 15, .end = RouteLegEnd::WALL};
const RouteLogLeg NORTH_OPEN{.action = RouteLegAction::WALK, .direction = Direction::NORTH, .tiles = 8, .end = RouteLegEnd::OPEN};

const RouteLogLeg SOUTH_WARP{.action = RouteLegAction::WALK, .direction = Direction::SOUTH, .tiles = 1, .end = RouteLegEnd::WARP};
const RouteLogLeg SURF_EAST{.action = RouteLegAction::SURF, .direction = Direction::EAST, .tiles = 0, .end = RouteLegEnd::OPEN};
const RouteLogLeg FACE_WEST{.action = RouteLegAction::FACE, .direction = Direction::WEST, .tiles = 0, .end = RouteLegEnd::OPEN};

struct LineCase{
    const char* name;
    std::string line;
    const char* expected;
};

UnitTestResult check(const std::vector<LineCase>& cases){
    for (const LineCase& c : cases){
        if (c.line != c.expected){
            return std::string(c.name) + ": expected \"" + c.expected + "\", got \"" + c.line + "\".";
        }
    }
    return true;
}

UnitTestResult test_legs(){
    return check({
        {.name = "to a wall", .line = to_string(EAST_TO_WALL), .expected = "east 15 (to wall)"},
        {.name = "open", .line = to_string(NORTH_OPEN), .expected = "north 8 (open)"},
        {.name = "onto a warp", .line = to_string(SOUTH_WARP), .expected = "south 1 (warp)"},
        {.name = "surf", .line = to_string(SURF_EAST), .expected = "surf east"},
        {.name = "face", .line = to_string(FACE_WEST), .expected = "face west"},
    });
}

UnitTestResult test_plans(){
    const std::vector<PlannedRouteLeg> bdsp_legs{
        {.leg = EAST_TO_WALL, .after = BDSP_WALL},
        {.leg = NORTH_OPEN, .after = BDSP_GRASS},
    };
    const std::vector<PlannedRouteLeg> frlg_legs{{.leg = NORTH_OPEN, .after = FRLG_NORTH}};
    return check({
        {
            .name = "plan from a set of tiles",
            .line = route_plan_line("To the grass", BDSP_FROM, bdsp_legs),
            .expected = "To the grass: plan from Route 202 (15, 25), Route 202 (16, 25):\n"
                "    east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)\n"
                "    north 8 (open) to Route 202 (23, 17)",
        },
        {
            .name = "plan from one tile",
            .line = route_plan_line("00.05/2 below_mart_door", FRLG_TILE, frlg_legs),
            .expected = "00.05/2 below_mart_door: plan from Route1 (12, 21):\n    north 8 (open) to Route1 (12, 13)",
        },
        {.name = "plan from an unknown tile", .line = route_plan_line("x", {}, {}), .expected = "x: plan from an unknown tile:"},
        {
            .name = "leg as it starts",
            .line = route_leg_line(1, 5, bdsp_legs[0]),
            .expected = "Leg 1/5: east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)",
        },
    });
}

UnitTestResult test_leg_ends(){
    return check({
        {
            .name = "walked",
            .line = route_leg_end_line("To the grass", EAST_TO_WALL, BDSP_WALL, EndBasis::WALKED, ""),
            .expected = "To the grass: east 15 (to wall): ended at Route 202 (23, 25), Route 202 (30, 26) (walked).",
        },
        {
            .name = "assumed",
            .line = route_leg_end_line("r", NORTH_OPEN, FRLG_TILE, EndBasis::ASSUMED, "after a wild battle"),
            .expected = "r: north 8 (open): ended at Route1 (12, 21) (assumed: after a wild battle).",
        },
        {
            .name = "unknown",
            .line = route_leg_end_line("r", NORTH_OPEN, {}, EndBasis::UNKNOWN, "stopped by a wild battle"),
            .expected = "r: north 8 (open): ended at an unknown tile (unknown: stopped by a wild battle).",
        },
    });
}

UnitTestResult test_events(){
    return check({
        {
            .name = "replan where seen",
            .line = route_replan_line("To the grass", EAST_TO_WALL, "stopped early after about 3 tiles", BDSP_FROM),
            .expected = "To the grass: east 15 (to wall): stopped early after about 3 tiles, at Route 202 (15, 25), Route 202 (16, 25); planning again.",
        },
        {.name = "found again", .line = route_found_line("r", FRLG_TILE, 2), .expected = "r: found again at Route1 (12, 21), after 2 hold(s)."},
        {
            .name = "trainer",
            .line = route_trainer_line("To the grass", "Youngster Tristan", TRAINER_SEEN),
            .expected = "To the grass: Youngster Tristan stopped us about Route 202 (20, 10).",
        },
        {
            .name = "arrived",
            .line = route_arrived_line("To the grass", ARRIVED),
            .expected = "To the grass: arrived at Route 202 (20, 25).",
        },
    });
}

}


void add_tests_RouteLog(UnitTestDatabase& database){
    database.add<Test_RouteLog>("legs", test_legs);
    database.add<Test_RouteLog>("plans, from a set of tiles or one", test_plans);
    database.add<Test_RouteLog>("leg ends: walked, assumed, unknown", test_leg_ends);
    database.add<Test_RouteLog>("replans, finds, trainers, arrivals", test_events);
}



}
}
