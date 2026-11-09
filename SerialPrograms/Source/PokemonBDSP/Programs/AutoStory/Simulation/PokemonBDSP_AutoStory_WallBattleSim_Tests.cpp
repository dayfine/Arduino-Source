/*  BDSP AutoStory Grid Simulation - Wild Battles on Wall Legs Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A wild battle on a leg to a wall, walked by the route walker on the grid
 *  simulation: however the escape ends the battle, the leg must still end at
 *  the wall with the battle counted.
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryExecutor.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_WallBattleSim_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::TilePos;


struct WallBattleInput{
    std::string name;
    SimWildBattle battle;
};
struct WallBattleExpectation{
    //  How the simulation notes the battle ended.
    std::string ended;
};
struct WallBattleCase{
    WallBattleInput input;
    WallBattleExpectation expectation;
};

StoryWorld make_world(const std::vector<std::string>& rows){
    Pokemon::MapData map;
    map.name = "Test";
    map.tiles = Pokemon::Grid<Pokemon::TileKind>{(int)rows[0].size(), (int)rows.size(), {}};
    for (const std::string& row : rows){
        for (char glyph : row){
            map.tiles.cells.push_back(*Pokemon::tile_from_glyph(glyph));
        }
    }
    StoryWorld world;
    world.add("Test", map, {0, 0});
    return world;
}

//  The corridor's one leg, east to its end wall, walked with `battle` on
//  the way; the walk's notes, or why it did not end at the wall.
struct WallBattleWalk{
    std::vector<std::string> notes;
    std::string error;
};
WallBattleWalk walk_wall_battle(const std::string& leg, const SimWildBattle& battle){
    StoryWorld world = make_world({
        "########################",
        "#......................#",
        "########################",
    });
    const TilePos start{1, 1};
    const TilePos target{22, 1};
    GridSim sim({.world = &world}, start, Direction::EAST, true);
    WallBattleWalk walk;
    GridSimRouteIO sim_io(sim, walk.notes, {}, {{.label = leg, .direction = Direction::EAST, .battle = battle}});
    RouteTrainers trainers{{}, [](const std::string&){}};
    try{
        walk_route(sim_io, world, {start}, trainers, {
            .goal = [&](const TilePos& pos){ return pos == target; },
            .label = "wall leg battle",
            .avoid = {},
            .costs = *story_route_costs("town"),
            .speed = MoveSpeed::RUN,
        });
    }catch (const GridSimFailure& f){
        walk.error = "failed: " + f.message;
        return walk;
    }
    if (nearest_tile(sim.player().pos) != target){
        walk.error = "ended at " + to_string(sim.player().pos) + ", not at the wall.";
    }
    return walk;
}

class Test_WallBattleSim : public UnitTest{
public:
    explicit Test_WallBattleSim(WallBattleCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Grid sim: a wild battle on a wall leg: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string leg = "wall leg battle: " + to_string(TileLeg{.direction = Direction::EAST, .tiles = 21, .ends_at_wall = true});
        const WallBattleWalk walk = walk_wall_battle(leg, m_case.input.battle);
        if (!walk.error.empty()){
            return walk.error;
        }
        const std::string ended = std::format("[Sim] {}: wild battle {}.", leg, m_case.expectation.ended);
        if (std::find(walk.notes.begin(), walk.notes.end(), ended) == walk.notes.end()){
            std::string notes;
            for (const std::string& line : walk.notes){
                notes += "\n    " + line;
            }
            return std::format("expected the note \"{}\"; the walk noted:{}", ended, notes);
        }
        return true;
    }

private:
    WallBattleCase m_case;
};

const WallBattleCase WALL_BATTLE_SIM_CASES[] = {
    {.input = {.name = "the first Run works", .battle = {.run_refusals = 0}}, .expectation = {.ended = "run from"}},
    {.input = {.name = "two refusals, then the third Run works", .battle = {.run_refusals = 2}}, .expectation = {.ended = "run from"}},
    //  console B, BDSP 01.03 run 2026-11-07: Onix's Bind in Oreburgh Mine
    //  refused nine Runs in 60 s and escape_wild_battle() gave up.
    {.input = {.name = "bound: every Run refused", .battle = {.run_refusals = 20}}, .expectation = {.ended = "fought to its end"}},
};

} // namespace


void add_tests_AutoStory_WallBattleSim(UnitTestDatabase& database){
    for (const WallBattleCase& item : WALL_BATTLE_SIM_CASES){
        database.add<Test_WallBattleSim>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
