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
#include "Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "Data/PokemonBDSP_AutoStory_TrainerSight.h"
#include "Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
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
    TilePos from = {};
    Direction direction = Direction::EAST;
    TilePos stop = {};
    size_t tiles = 0;
    bool anchored = false;
};

UnitTestResult test_walk_to_wall(Logger&){
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


//  plan_tile_route() around a pillar: two legs, both to a wall, that trace
//  to the goal.
UnitTestResult test_plan_tile_route(Logger& logger){
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


//  plan_robust_route() from one or several tiles to a goal, with the open
//  legs' error as the costs give it.
struct RobustCase{
    std::string name;
    std::vector<TilePos> from;
    TilePos goal;
    RobustPlanCosts costs = {};
    //  The plan as to_string() writes it, or empty when there is none.
    std::string legs;
};

UnitTestResult test_plan_robust_route(Logger&){
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


//  Empty map name selects a synthetic corridor: ####### / #.....# / #######.
//  Both possible starts (1,1)/(5,1) must share a safe wall ray. NPCs and
//  warps on that ray or its wall invalidate it; ordinary terrain is unchanged.
struct RecoveryGeometryInput{
    std::string name;
    std::string map = {};
    std::vector<TilePos> from;
    std::optional<Pokemon::TileBox> wander = {};
    std::vector<TilePos> warps = {};
};
struct RecoveryGeometryExpectation{
    std::optional<TilePos> anchor;
    Direction direction = Direction::SOUTH;
    size_t holds = 1;
    std::optional<TilePos> free_tile = {};
};
struct RecoveryGeometryCase{
    RecoveryGeometryInput input;
    RecoveryGeometryExpectation expectation;
};

Pokemon::MapData recovery_map(const RecoveryGeometryInput& input){
    Pokemon::MapData map;
    if (!input.map.empty()){
        return story_map(input.map);
    }
    map.name = "RecoveryDiagnostic";
    map.tiles = Pokemon::Grid<Pokemon::TileKind>{7, 3, {}};
    for (std::string_view row : {"#######", "#.....#", "#######"}){
        for (char glyph : std::string(row)){
            map.tiles.cells.push_back(*Pokemon::tile_from_glyph(glyph));
        }
    }
    if (input.wander){
        map.objects.push_back({.name = "wanderer", .x = 3, .y = 1, .facing = std::nullopt,
            .wander = input.wander, .trainer = std::nullopt, .hide_flag = std::nullopt});
    }
    for (const auto& warp : input.warps){
        map.warps.push_back({.x = warp.x, .y = warp.y, .to = std::nullopt});
    }
    return map;
}

class Test_RecoveryGeometry : public UnitTest{
public:
    explicit Test_RecoveryGeometry(RecoveryGeometryCase scenario)
        : UnitTest("PokemonBDSP::AutoStory - Recovery geometry: " + scenario.input.name), m_case(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const auto& expected = m_case.expectation;
        const auto map = recovery_map(m_case.input);
        StoryWorld world;
        world.add(map.name, map, {0, 0});
        if (expected.free_tile && (!standable(world, *expected.free_tile)
            || world.occupied(*expected.free_tile) != StoryWorld::NOBODY)){
            return "Expected ordinary movement to remain free at " + Pokemon::to_string(*expected.free_tile);
        }
        const auto anchors = wall_recovery_candidates(world, m_case.input.from);
        const auto actual = anchors.empty() ? std::optional<TilePos>() : std::optional<TilePos>(anchors.front().at);
        if (actual != expected.anchor){
            return std::format("Expected anchor {}, got {}.",
                expected.anchor ? Pokemon::to_string(*expected.anchor) : "none",
                actual ? Pokemon::to_string(*actual) : "none");
        }
        if (anchors.empty()){
            return true;
        }
        const auto& holds = anchors.front().holds;
        if (holds.size() != expected.holds || holds.empty() || holds.front().direction != expected.direction){
            return std::format("Expected {} holds beginning {}, got {} beginning {}.",
                expected.holds, Pokemon::direction_name(expected.direction), holds.size(),
                holds.empty() ? "none" : Pokemon::direction_name(holds.front().direction));
        }
        return true;
    }
private:
    RecoveryGeometryCase m_case;
};

//  A trainer's sight stops at the first tile the player cannot stand on,
//  and a leg through it is reported; a leg that is stopped throws.
UnitTestResult test_trainer_sight(Logger&){
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


struct RoutePlannerInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct RoutePlannerExpectation{
};

struct RoutePlannerCase{
    RoutePlannerInput input;
    RoutePlannerExpectation expectation;
};

class Test_RoutePlanner : public UnitTest{
public:
    explicit Test_RoutePlanner(RoutePlannerCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Route planner: " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    RoutePlannerCase m_case;
};

} // namespace


const RoutePlannerCase ROUTE_PLANNER_CASES[] = {
    {
        .input = {"walking until stopped", test_walk_to_wall},
        .expectation = {},
    },
    {
        .input = {"legs to walls around a pillar", test_plan_tile_route},
        .expectation = {},
    },
    {
        .input = {"robust plans", test_plan_robust_route},
        .expectation = {},
    },
    {
        .input = {"trainers' sight lines", test_trainer_sight},
        .expectation = {},
    },
};

const RecoveryGeometryCase GEOMETRY_CASES[] = {
    {.input = {.name = "mine rows converge south", .map = "OreburghMine", .from = {{15, 15}, {15, 16}, {15, 17}}},
        .expectation = {.anchor = TilePos{.x = 15, .y = 17}}},
    {.input = {.name = "r55 tiles converge east", .map = "Route202", .from = {{27, 5}, {26, 5}}},
        .expectation = {.anchor = TilePos{.x = 30, .y = 5}, .direction = Direction::EAST}},
    {.input = {.name = "wandering area is unsafe", .from = {{1, 1}, {5, 1}},
            .wander = Pokemon::TileBox{.x0 = 2, .y0 = 1, .x1 = 4, .y1 = 1}},
        .expectation = {.anchor = std::nullopt, .free_tile = TilePos{.x = 3, .y = 1}}},
    {.input = {.name = "warp crossings are unsafe", .from = {{1, 1}, {5, 1}}, .warps = {{3, 1}}},
        .expectation = {.anchor = std::nullopt}},
    {.input = {.name = "warps on blocked glyphs are unsafe", .from = {{1, 1}, {5, 1}}, .warps = {{0, 1}, {6, 1}}},
        .expectation = {.anchor = std::nullopt}},
};

void add_tests_AutoStory_RoutePlanner(UnitTestDatabase& database){
    for (const auto& item : ROUTE_PLANNER_CASES){
        database.add<Test_RoutePlanner>(item);
    }
    for (const auto& scenario : GEOMETRY_CASES){
        database.add<Test_RecoveryGeometry>(scenario);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
