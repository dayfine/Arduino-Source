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

const RouteLogLeg EAST_TO_WALL{.action = RouteLegAction::WALK, .direction = Direction::EAST, .tiles = 15, .end = RouteLegEnd::WALL};
const RouteLogLeg NORTH_OPEN{.action = RouteLegAction::WALK, .direction = Direction::NORTH, .tiles = 8, .end = RouteLegEnd::OPEN};

struct LineCase{
    const char* name;
    std::string line;
    const char* expected;
};

std::vector<LineCase> line_cases(){
    return {
        {.name = "leg to a wall", .line = to_string(EAST_TO_WALL), .expected = "east 15 (to wall)"},
        {.name = "open leg", .line = to_string(NORTH_OPEN), .expected = "north 8 (open)"},
        {
            .name = "leg onto a warp",
            .line = to_string(RouteLogLeg{.action = RouteLegAction::WALK, .direction = Direction::SOUTH, .tiles = 1, .end = RouteLegEnd::WARP}),
            .expected = "south 1 (warp)",
        },
        {
            .name = "surf",
            .line = to_string(RouteLogLeg{.action = RouteLegAction::SURF, .direction = Direction::EAST, .tiles = 0, .end = RouteLegEnd::OPEN}),
            .expected = "surf east",
        },
        {
            .name = "face",
            .line = to_string(RouteLogLeg{.action = RouteLegAction::FACE, .direction = Direction::WEST, .tiles = 0, .end = RouteLegEnd::OPEN}),
            .expected = "face west",
        },
        {
            .name = "plan from a set of tiles",
            .line = route_plan_line("To the grass", BDSP_FROM, {
                {.leg = EAST_TO_WALL, .after = BDSP_WALL},
                {.leg = NORTH_OPEN, .after = {{"Route 202", {23, 17}}}},
            }),
            .expected =
                "To the grass: plan from Route 202 (15, 25), Route 202 (16, 25):\n"
                "    east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)\n"
                "    north 8 (open) to Route 202 (23, 17)",
        },
        {
            .name = "plan from one tile",
            .line = route_plan_line("00.05/2 below_mart_door", FRLG_TILE, {{.leg = NORTH_OPEN, .after = {{"Route1", {12, 13}}}}}),
            .expected =
                "00.05/2 below_mart_door: plan from Route1 (12, 21):\n"
                "    north 8 (open) to Route1 (12, 13)",
        },
        {
            .name = "plan from an unknown tile",
            .line = route_plan_line("x", {}, {}),
            .expected = "x: plan from an unknown tile:",
        },
        {
            .name = "leg as it starts",
            .line = route_leg_line(1, 5, {.leg = EAST_TO_WALL, .after = BDSP_WALL}),
            .expected = "Leg 1/5: east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)",
        },
        {
            .name = "leg walked",
            .line = route_leg_end_line("To the grass", EAST_TO_WALL, BDSP_WALL, EndBasis::WALKED, ""),
            .expected = "To the grass: east 15 (to wall): ended at Route 202 (23, 25), Route 202 (30, 26) (walked).",
        },
        {
            .name = "leg assumed",
            .line = route_leg_end_line("r", NORTH_OPEN, FRLG_TILE, EndBasis::ASSUMED, "after a wild battle"),
            .expected = "r: north 8 (open): ended at Route1 (12, 21) (assumed: after a wild battle).",
        },
        {
            .name = "leg end unknown",
            .line = route_leg_end_line("r", NORTH_OPEN, {}, EndBasis::UNKNOWN, "stopped by a wild battle"),
            .expected = "r: north 8 (open): ended at an unknown tile (unknown: stopped by a wild battle).",
        },
        {
            .name = "replan where seen",
            .line = route_replan_line("To the grass", EAST_TO_WALL, "stopped early after about 3 tiles", BDSP_FROM),
            .expected = "To the grass: east 15 (to wall): stopped early after about 3 tiles, at Route 202 (15, 25), Route 202 (16, 25); planning again.",
        },
        {
            .name = "found again",
            .line = route_found_line("r", FRLG_TILE, 2),
            .expected = "r: found again at Route1 (12, 21), after 2 hold(s).",
        },
        {
            .name = "trainer",
            .line = route_trainer_line("To the grass", "Youngster Tristan", {{"Route 202", {20, 10}}}),
            .expected = "To the grass: Youngster Tristan stopped us about Route 202 (20, 10).",
        },
        {
            .name = "arrived",
            .line = route_arrived_line("To the grass", {{"Route 202", {20, 25}}}),
            .expected = "To the grass: arrived at Route 202 (20, 25).",
        },
    };
}

UnitTestResult test_lines(){
    for (const LineCase& c : line_cases()){
        if (c.line != c.expected){
            return std::string(c.name) + ": expected \"" + c.expected + "\", got \"" + c.line + "\".";
        }
    }
    return true;
}

}


void add_tests_RouteLog(UnitTestDatabase& database){
    database.add<Test_RouteLog>("lines, word for word", test_lines);
}



}
}
