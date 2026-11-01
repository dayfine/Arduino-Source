/*  BDSP AutoStory Route Planner Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The route planner (Data/PokemonBDSP_AutoStory_RoutePlanner.h) on small
 *  synthetic maps: walks to a wall, plans that end on walls, robust plans
 *  that bring several possible tiles together, and trainers' sight lines.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_RoutePlanner_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::TilePos;

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

std::string describe(const std::vector<PlannedLeg>& legs){
    std::string text;
    for (const PlannedLeg& leg : legs){
        text += (text.empty() ? "" : "; ") + to_string(leg);
    }
    return text;
}


struct WallCase{
    std::string name;
    std::vector<std::string> rows;
    TilePos from;
    Direction direction = Direction::EAST;
    TilePos stop;
    size_t tiles = 0;
    bool anchored = false;
};

class Test_WalkToWall : public UnitTest{
public:
    Test_WalkToWall()
        : UnitTest("PokemonBDSP::AutoStory - Route planner: walking until stopped")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<WallCase> cases{
            {.name = "to a wall", .rows = {"....#"}, .stop = {3, 0}, .tiles = 3, .anchored = true},
            {.name = "off the map", .rows = {"...."}, .stop = {3, 0}, .tiles = 3, .anchored = false},
            {.name = "into a door", .rows = {"..D"}, .stop = {1, 0}, .tiles = 1, .anchored = false},
            {.name = "over a ledge, which counts 2", .rows = {".", "v", ".", "#"}, .direction = Direction::SOUTH,
                .stop = {0, 2}, .tiles = 2, .anchored = true},
        };
        for (const WallCase& item : cases){
            const WallHit hit = slide_to_wall(make_world(item.rows), item.from, item.direction);
            if (!(hit.stop == item.stop) || hit.tiles != item.tiles || hit.anchored != item.anchored){
                return std::format("{}: stops at {} after {} tiles, anchored {}; expected {} after {}, anchored {}.", item.name,
                    Pokemon::to_string(hit.stop), hit.tiles, hit.anchored, Pokemon::to_string(item.stop), item.tiles, item.anchored);
            }
        }
        return true;
    }
};


//  plan_tile_route() around a pillar: two legs, both to a wall, that trace
//  to the goal.
class Test_PlanTileRoute : public UnitTest{
public:
    Test_PlanTileRoute()
        : UnitTest("PokemonBDSP::AutoStory - Route planner: legs to walls around a pillar")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld world = make_world({"#####", "#...#", "#.#.#", "#...#", "#####"});
        const std::optional<std::vector<TileLeg>> legs = plan_tile_route(world, {1, 1}, {3, 3});
        if (!legs || legs->size() != 2 || !(*legs)[0].ends_at_wall || !(*legs)[1].ends_at_wall){
            return std::string("Expected two legs, both to a wall.");
        }
        if (!(trace_legs(world, {1, 1}, *legs).back() == TilePos{3, 3})){
            return std::string("The legs do not end on (3, 3).");
        }
        logger.log(std::format("{}, {}", to_string((*legs)[0]), to_string((*legs)[1])));
        const std::optional<std::vector<TileLeg>> none = plan_tile_route(world, {1, 1}, {1, 1});
        if (!none || !none->empty()){
            return std::string("A route to the start is not empty.");
        }
        return true;
    }
};


//  plan_robust_route() from one or several tiles to a goal, with the open
//  legs' error as the costs give it.
struct RobustCase{
    std::string name;
    std::vector<TilePos> from;
    TilePos goal;
    RobustPlanCosts costs;
    //  The plan as to_string() writes it, or empty when there is none.
    std::string legs;
};

class Test_PlanRobustRoute : public UnitTest{
public:
    Test_PlanRobustRoute()
        : UnitTest("PokemonBDSP::AutoStory - Route planner: robust plans")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StoryWorld world = make_world({"######", "#....#", "######"});
        RobustPlanCosts exact_step;
        exact_step.exact_open_tiles = 1;
        const std::vector<RobustCase> cases{
            {.name = "two starts meet at the wall", .from = {{1, 1}, {2, 1}}, .goal = {4, 1}, .legs = "east 3 (to wall) to (4, 1)"},
            {.name = "an open stop is never sure", .from = {{1, 1}}, .goal = {3, 1}, .legs = ""},
            {.name = "unless a one-tile step is exact", .from = {{1, 1}}, .goal = {3, 1}, .costs = exact_step,
                .legs = "east 3 (to wall) to (4, 1); west 1 (open) to (3, 1)"},
        };
        for (const RobustCase& item : cases){
            const auto legs = plan_robust_route(world, item.from, [&](const TilePos& pos){ return pos == item.goal; }, item.costs);
            const std::string read = legs ? describe(*legs) : "";
            if (read != item.legs){
                return std::format("{}: planned \"{}\", expected \"{}\".", item.name, read, item.legs);
            }
        }
        try{
            plan_robust_route(world, TilePos{0, 0}, TilePos{4, 1});
            return std::string("A start on a wall did not throw.");
        }catch (InternalProgramError&){}
        return true;
    }
};


//  A trainer's sight stops at the first tile the player cannot stand on,
//  and a leg through it is reported; a leg that is stopped throws.
class Test_TrainerSight : public UnitTest{
public:
    Test_TrainerSight()
        : UnitTest("PokemonBDSP::AutoStory - Route planner: trainers' sight lines")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StoryWorld world = make_world({".....", ".....", "..#.."});
        const TrainerSight trainer{.name = "lass", .pos = {2, 0}, .facing = Direction::SOUTH, .range = 4};
        if (sight_tiles(world, trainer) != std::vector<TilePos>{{2, 1}}){
            return std::string("The lass's sight is not the one tile above the rock.");
        }
        const std::vector<TileLeg> legs{{Direction::EAST, 4, true}, {Direction::SOUTH, 1, false}};
        const auto crossings = legs_crossing_sight(world, {0, 1}, legs, {trainer});
        if (crossings != std::vector<std::vector<std::string>>{{"lass"}, {}}){
            return std::string("Only the first leg crosses the lass's sight.");
        }
        try{
            trace_legs(world, {0, 1}, {{Direction::SOUTH, 2, false}});
            return std::string("A leg off the map did not throw.");
        }catch (InternalProgramError&){}
        return true;
    }
};

}


void add_tests_AutoStory_RoutePlanner(UnitTestDatabase& database){
    database.add<Test_WalkToWall>();
    database.add<Test_PlanTileRoute>();
    database.add<Test_PlanRobustRoute>();
    database.add<Test_TrainerSight>();
}


}
}
}
