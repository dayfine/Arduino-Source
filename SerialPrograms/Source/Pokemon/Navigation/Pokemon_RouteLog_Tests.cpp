/*  Route Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The route log lines, word for word: BDSP-style position sets (several
 *  tiles, map names with spaces) and FRLG-style single tiles.
 *
 */

#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_RouteLog.h"
#include "Pokemon_RouteLog_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

struct RouteLogTestCase{
    struct Input{
        std::string name;
        std::string line;
    };
    struct Expectation{
        std::string expected;
    };
    Input input;
    Expectation expectation;
};

class Test_RouteLog : public UnitTest{
public:
    explicit Test_RouteLog(RouteLogTestCase scenario)
        : UnitTest(std::format("Pokemon::RouteLog - {}", scenario.input.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (m_scenario.input.line != m_scenario.expectation.expected){
            return std::format(
                "{}: expected \"{}\", got \"{}\".",
                m_scenario.input.name, m_scenario.expectation.expected, m_scenario.input.line
            );
        }
        return true;
    }

private:
    RouteLogTestCase m_scenario;
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

const std::vector<PlannedRouteLeg> BDSP_LEGS{
    {.leg = EAST_TO_WALL, .after = BDSP_WALL},
    {.leg = NORTH_OPEN, .after = BDSP_GRASS},
};
const std::vector<PlannedRouteLeg> FRLG_LEGS{{.leg = NORTH_OPEN, .after = FRLG_NORTH}};

const RouteLogTestCase CASES[] = {
    {
        .input = {.name = "to a wall", .line = to_string(EAST_TO_WALL)},
        .expectation = {.expected = "east 15 (to wall)"},
    },
    {
        .input = {.name = "open", .line = to_string(NORTH_OPEN)},
        .expectation = {.expected = "north 8 (open)"},
    },
    {
        .input = {.name = "onto a warp", .line = to_string(SOUTH_WARP)},
        .expectation = {.expected = "south 1 (warp)"},
    },
    {
        .input = {.name = "surf", .line = to_string(SURF_EAST)},
        .expectation = {.expected = "surf east"},
    },
    {
        .input = {.name = "face", .line = to_string(FACE_WEST)},
        .expectation = {.expected = "face west"},
    },
    {
        .input = {
            .name = "plan from a set of tiles",
            .line = route_plan_line("To the grass", BDSP_FROM, BDSP_LEGS),
        },
        .expectation = {
            .expected = "To the grass: plan from Route 202 (15, 25), Route 202 (16, 25):"
                " east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26);"
                " north 8 (open) to Route 202 (23, 17)",
        },
    },
    {
        .input = {
            .name = "plan from one tile",
            .line = route_plan_line("00.05/2 below_mart_door", FRLG_TILE, FRLG_LEGS),
        },
        .expectation = {
            .expected = "00.05/2 below_mart_door: plan from Route1 (12, 21): north 8 (open) to Route1 (12, 13)",
        },
    },
    {
        .input = {.name = "plan from an unknown tile", .line = route_plan_line("x", {}, {})},
        .expectation = {.expected = "x: plan from an unknown tile:"},
    },
    {
        .input = {
            .name = "leg as it starts",
            .line = route_leg_line(1, 5, BDSP_LEGS[0]),
        },
        .expectation = {
            .expected = "Leg 1/5: east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)",
        },
    },
    {
        .input = {
            .name = "walked",
            .line = route_leg_end_line("To the grass", EAST_TO_WALL, BDSP_WALL, EndBasis::WALKED, ""),
        },
        .expectation = {
            .expected = "To the grass: east 15 (to wall): ended at Route 202 (23, 25), Route 202 (30, 26) (walked).",
        },
    },
    {
        .input = {
            .name = "assumed",
            .line = route_leg_end_line("r", NORTH_OPEN, FRLG_TILE, EndBasis::ASSUMED, "after a wild battle"),
        },
        .expectation = {
            .expected = "r: north 8 (open): ended at Route1 (12, 21) (assumed: after a wild battle).",
        },
    },
    {
        .input = {
            .name = "unknown",
            .line = route_leg_end_line("r", NORTH_OPEN, {}, EndBasis::UNKNOWN, "stopped by a wild battle"),
        },
        .expectation = {
            .expected = "r: north 8 (open): ended at an unknown tile (unknown: stopped by a wild battle).",
        },
    },
    {
        .input = {
            .name = "replan where seen",
            .line = route_replan_line("To the grass", EAST_TO_WALL, "stopped early after about 3 tiles", BDSP_FROM),
        },
        .expectation = {
            .expected = "To the grass: east 15 (to wall): stopped early after about 3 tiles, at Route 202 (15, 25), Route 202 (16, 25); planning again.",
        },
    },
    {
        .input = {.name = "found again", .line = route_found_line("r", FRLG_TILE, 2)},
        .expectation = {.expected = "r: found again at Route1 (12, 21), after 2 hold(s)."},
    },
    {
        .input = {
            .name = "trainer",
            .line = route_trainer_line("To the grass", "Youngster Tristan", TRAINER_SEEN),
        },
        .expectation = {
            .expected = "To the grass: Youngster Tristan stopped us about Route 202 (20, 10).",
        },
    },
    {
        .input = {
            .name = "arrived",
            .line = route_arrived_line("To the grass", ARRIVED),
        },
        .expectation = {
            .expected = "To the grass: arrived at Route 202 (20, 25).",
        },
    },
};

} // namespace


void add_tests_RouteLog(UnitTestDatabase& database){
    for (const auto& scenario : CASES){
        database.add<Test_RouteLog>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
