/*  BDSP AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_JubilifeCity.h"
#include "Data/PokemonBDSP_AutoStory_OreburghMine.h"
#include "Data/PokemonBDSP_AutoStory_RavagedPath.h"
#include "Data/PokemonBDSP_AutoStory_Route203.h"
#include "Data/PokemonBDSP_AutoStory_Route204.h"
#include "Data/PokemonBDSP_AutoStory_Route204South.h"
#include "Data/PokemonBDSP_AutoStory_RouteRecords.h"
#include "Data/PokemonBDSP_AutoStory_Route201.h"
#include "Data/PokemonBDSP_AutoStory_Route202.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Utils/PokemonBDSP_AutoStory_Location.h"
#include "Utils/PokemonBDSP_AutoStory_NewGame.h"
#include "Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "StorySegments/PokemonBDSP_AutoStory_SandgemCenterEvents.h"
#include "PokemonBDSP_AutoStory_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::TileGrid;
using Pokemon::TileLeg;
using Pokemon::TilePos;
using Pokemon::TileType;

//  A small map with every tile type.
//  Row 2 is a ledge with a gap at x = 4. The bottom row has a sign at (1,4),
//  an NPC at (3,4) and a door at (5,4); (3,0) is off the map.
const TileGrid& small_grid(){
    static const TileGrid grid({
        R"(#..?..#)",
        R"(#.."".#)",
        R"(#vvv..#)",
        R"(#.....#)",
        R"(#S.N.D#)",
    });
    return grid;
}

class Test_TileGridParse : public UnitTest{
public:
    Test_TileGridParse()
        : UnitTest("PokemonBDSP::AutoStory - tile grid parse")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const TileGrid& grid = small_grid();
        if (grid.width() != 7 || grid.height() != 5){
            return "Small grid is " + std::to_string(grid.width()) + "x" + std::to_string(grid.height()) + ".";
        }
        struct Case{
            TilePos pos;
            TileType type;
        };
        const Case cases[] = {
            {{0, 0}, TileType::BLOCKED},
            {{1, 0}, TileType::WALKABLE},
            {{3, 1}, TileType::TALL_GRASS},
            {{2, 2}, TileType::LEDGE},
            {{3, 4}, TileType::NPC},
            {{1, 4}, TileType::SIGN},
            {{5, 4}, TileType::DOOR},
            {{3, 0}, TileType::UNKNOWN},
            {{-1, 0}, TileType::UNKNOWN},
            {{7, 4}, TileType::UNKNOWN},
        };
        for (const Case& c : cases){
            if (grid.at(c.pos) != c.type){
                return "Tile " + Pokemon::to_string(c.pos) + " parsed as " + std::to_string((int)grid.at(c.pos)) + ".";
            }
        }
        for (const std::vector<std::string>& bad : {
            std::vector<std::string>{"...", ".."},
            std::vector<std::string>{"..x"},
        }){
            try{
                TileGrid parsed(bad);
                return "A malformed grid was accepted.";
            }catch (InternalProgramError&){}
        }

        if (grid.rows()[4] != "#S.N.D#"){
            return "Small grid row 4 prints as " + grid.rows()[4] + ".";
        }

        struct Map{
            const char* name;
            const TileGrid& grid;
            int width;
            int height;
        };
        const Map maps[] = {
            {MAP_ROUTE_201,     Route201::grid(),       64, 20},
            {MAP_SANDGEM_TOWN,  SandgemTown::grid(),    34, 28},
            {MAP_ROUTE_202,     Route202::grid(),       36, 26},
        };
        for (const Map& map : maps){
            if (map.grid.width() != map.width || map.grid.height() != map.height){
                return std::string(map.name) + " is " + std::to_string(map.grid.width()) + "x" + std::to_string(map.grid.height()) + ".";
            }
        }

        struct Named{
            const char* map;
            TilePos pos;
            TileType type;
        };
        const Named named[] = {
            {MAP_SANDGEM_TOWN,  SandgemTown::LAB_DOOR,              TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKEMON_CENTER_DOOR,   TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKEMON_CENTER_SIGN,   TileType::SIGN},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKE_MART_DOOR,        TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::FRONT_ROW_EAST_END,    TileType::WALKABLE},
            {MAP_ROUTE_202,     Route202::CATCH_SPOT,               TileType::TALL_GRASS},
            {MAP_ROUTE_202,     Route202::SAVE_SPOT,                TileType::WALKABLE},
            {MAP_ROUTE_202,     Route202::TRAINER_A,                TileType::NPC},
            {MAP_ROUTE_202,     Route202::TRAINER_B,                TileType::NPC},
            {MAP_ROUTE_202,     Route202::TRAINER_C,                TileType::NPC},
            {MAP_ROUTE_202,     Route202::TRAINER_D,                TileType::NPC},
            {MAP_ROUTE_202,     Route202::EXIT_JUBILIFE,            TileType::WALKABLE},
        };
        const Pokemon::MapSet& world = story_maps();
        for (const Named& c : named){
            const TilePos pos = world.to_world(c.map, c.pos);
            if (world.world().at(pos) != c.type){
                return std::string(c.map) + " tile " + Pokemon::to_string(c.pos) + " is type " +
                    std::to_string((int)world.world().at(pos)) + " in the joined maps.";
            }
            const std::optional<Pokemon::MapTile> local = world.to_local(pos);
            if (!local || local->map != c.map || local->pos != c.pos){
                return std::string(c.map) + " tile " + Pokemon::to_string(c.pos) + " does not map back.";
            }
        }
        //  Sandgem's Poke Mart is next door east of its Center, on the same
        //  front row: 00.04 walks from the one door to the other.
        {
            const TilePos center = world.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR);
            const TilePos mart = world.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR);
            if (center.y != mart.y || mart.x <= center.x){
                return "The Poke Mart's door is not east of the Pokemon Center's on the same row.";
            }
            for (int x = center.x; x <= mart.x; x++){
                if (!world.world().standable({x, mart.y + 1})){
                    return "The row below the Center's and the Mart's doors is not open at " + world.describe({x, mart.y + 1}) + ".";
                }
            }
        }

        //  The maps join where their exits meet.
        struct Join{
            const char* from_map;
            TilePos from;
            Direction direction;
            const char* to_map;
            TilePos to;
        };
        const Join joins[] = {
            {MAP_ROUTE_201,     {63, 4},    Direction::EAST,    MAP_SANDGEM_TOWN,   {0, 7}},
            {MAP_ROUTE_201,     {63, 6},    Direction::EAST,    MAP_SANDGEM_TOWN,   {0, 9}},
            {MAP_SANDGEM_TOWN,  {24, 0},    Direction::NORTH,   MAP_ROUTE_202,      {25, 25}},
            {MAP_SANDGEM_TOWN,  {29, 0},    Direction::NORTH,   MAP_ROUTE_202,      {30, 25}},
        };
        for (const Join& join : joins){
            const TilePos from = world.to_world(join.from_map, join.from);
            const std::optional<TilePos> next = world.world().step(from, join.direction);
            if (!next || *next != world.to_world(join.to_map, join.to)){
                return "No step from " + world.describe(from) + " to " + join.to_map + " " + Pokemon::to_string(join.to) + ".";
            }
        }
        Pokemon::MapSet overlapping;
        overlapping.add("a", small_grid(), {0, 0});
        overlapping.add("b", small_grid(), {6, 4});    //  shares one blocked tile
        try{
            overlapping.add("c", small_grid(), {5, 4});
            return "Maps that overlap on a door were accepted.";
        }catch (InternalProgramError&){}
        return true;
    }
};

class Test_TileGridWallHit : public UnitTest{
public:
    Test_TileGridWallHit()
        : UnitTest("PokemonBDSP::AutoStory - tile grid wall hit")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            const TileGrid& grid;
            TilePos from;
            Direction direction;
            TilePos stop;
            size_t tiles;
            bool anchored;
        };
        const TileGrid& small = small_grid();
        const TileGrid& route = Route202::grid();
        const Case cases[] = {
            {small, {1, 1}, Direction::EAST,  {5, 1}, 4, true},     //  through grass to the east wall
            {small, {1, 0}, Direction::SOUTH, {1, 3}, 3, true},     //  hops the ledge, stops at the sign
            {small, {2, 3}, Direction::NORTH, {2, 3}, 0, true},     //  a ledge stops a north walk
            {small, {3, 3}, Direction::SOUTH, {3, 3}, 0, true},     //  the NPC is a wall
            {small, {4, 4}, Direction::NORTH, {4, 0}, 4, false},    //  through the gap, off the top edge
            {small, {4, 3}, Direction::EAST,  {5, 3}, 1, true},
            {small, {4, 4}, Direction::EAST,  {4, 4}, 0, false},    //  into the door
            {small, {2, 0}, Direction::EAST,  {2, 0}, 0, false},    //  off the map
            {route, Route202::CATCH_SPOT, Direction::WEST,  {3, 22}, 16, true},
            {route, Route202::CATCH_SPOT, Direction::NORTH, {19, 20}, 2, true},
            {route, Route202::CATCH_SPOT, Direction::EAST,  {30, 22}, 11, true},
            //  Measured on the console from the post-catch save.
            {route, {30, 22}, Direction::NORTH, {30, 19}, 3, true},
            {route, {30, 19}, Direction::WEST,  {22, 19}, 8, true},
            {route, {22, 19}, Direction::SOUTH, {22, 24}, 5, true},
            {route, {7, 12}, Direction::NORTH, {7, 12}, 0, true},  //  trainer_a holds the player
            //  After trainer_d, as measured in r75: the east wall is at column 31
            //  on row 16, and column 30 is open up to the trees above row 4.
            {route, {22, 16}, Direction::EAST,  {30, 16}, 8, true},
            {route, {30, 16}, Direction::NORTH, {30, 4}, 12, true},
            //  Through the gap east of the north ledge, over the south ledge
            //  and off the bottom edge to Sandgem Town.
            {route, {29, 4}, Direction::SOUTH, {29, 25}, 21, false},
            {route, {12, 8}, Direction::SOUTH, {12, 22}, 14, true},  //  hops both ledges
            {route, Route202::EXIT_JUBILIFE, Direction::NORTH, Route202::EXIT_JUBILIFE, 0, false},
        };
        auto check = [](const Case& c) -> std::string{
            const Pokemon::WallHit hit = Pokemon::walk_to_wall(c.grid, c.from, c.direction);
            if (hit.stop != c.stop || hit.tiles != c.tiles || hit.anchored != c.anchored){
                return "Walking " + std::string(Pokemon::direction_name(c.direction)) + " from " +
                    Pokemon::to_string(c.from) + " stops at " + Pokemon::to_string(hit.stop) + " after " +
                    std::to_string(hit.tiles) + " tiles" + (hit.anchored ? "" : " (not anchored)") +
                    ", expected " + Pokemon::to_string(c.stop) + " after " + std::to_string(c.tiles) + ".";
            }
            return "";
        };
        for (const Case& c : cases){
            const std::string error = check(c);
            if (!error.empty()){
                return error;
            }
        }
        //  Sandgem Town on the joined maps (00.04): the front row of the
        //  Center and the Mart runs east to the trees past the Mart, and the
        //  Mart's door is a warp, not a wall.
        const Pokemon::MapSet& maps = story_maps();
        const TileGrid& world = maps.world();
        auto town = [&](const TilePos& pos){ return maps.to_world(MAP_SANDGEM_TOWN, pos); };
        const TilePos below_mart = town(SandgemTown::POKE_MART_DOOR.next(Direction::SOUTH));
        const TilePos row_end = town(SandgemTown::FRONT_ROW_EAST_END);
        const Case sandgem[] = {
            {world, town(SandgemTown::POKEMON_CENTER_DOOR.next(Direction::SOUTH)), Direction::EAST, row_end, 12, true},
            {world, below_mart, Direction::EAST, row_end, 2, true},
            {world, below_mart, Direction::NORTH, below_mart, 0, false},
            {world, row_end, Direction::NORTH, row_end, 0, true},
        };
        for (const Case& c : sandgem){
            const std::string error = check(c);
            if (!error.empty()){
                return error;
            }
        }
        return true;
    }
};

//  Plans from `from` to `goal` allowing open legs a tile short or long, and
//  logs the legs with every map tile each may end on.
std::string check_robust_plan(
    Logger& logger, const std::string& name, const Pokemon::MapSet& maps,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal,
    const Pokemon::RobustPlanCosts& costs = Pokemon::RobustPlanCosts()
){
    const auto legs = Pokemon::plan_robust_route(maps.world(), from, goal, costs);
    if (!legs){
        return name + ": no robust route found.";
    }
    std::string text = name + " (robust): from";
    for (const TilePos& pos : from){
        text += " " + maps.describe(pos);
    }
    for (const Pokemon::PlannedLeg& leg : *legs){
        text += "\n    " + Pokemon::to_string(leg.leg) + " to";
        for (const TilePos& pos : leg.after){
            text += " " + maps.describe(pos);
        }
    }
    logger.log(text);
    const std::vector<TilePos>& end = legs->empty() ? from : legs->back().after;
    for (const TilePos& pos : end){
        if (!goal(pos)){
            return name + ": the robust plan may end on " + maps.describe(pos) + ".";
        }
    }
    return "";
}

//  Checks that no leg of a robust plan from `from` to `goal` can stop in tall
//  grass, where a walk cannot read that it has stopped (the sway keeps the
//  screen changing): a leg that only runs out of time there would be taken
//  as an anchor.
std::string check_plan_stops_readable(
    const std::string& name, const Pokemon::MapSet& maps,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal,
    const Pokemon::RobustPlanCosts& costs = Pokemon::RobustPlanCosts()
){
    const auto legs = Pokemon::plan_robust_route(maps.world(), from, goal, costs);
    if (!legs){
        return name + ": no robust route found.";
    }
    for (const Pokemon::PlannedLeg& leg : *legs){
        if (leg.leg.ends_at_wall && !route_wall_readable(maps.world(), leg.after)){
            return name + ": " + Pokemon::to_string(leg.leg) + " ends on a wall in tall grass, where a stop cannot be read.";
        }
    }
    return "";
}

//  Checks that no leg of a robust plan from `from` to `goal` can walk into a
//  door, from any tile the leg before may have left us on, even if an open
//  leg runs on to twice its length plus two: r123a's legs in Sandgem ran
//  past where the grid said and the last one entered a house.
std::string check_plan_avoids_doors(
    const std::string& name, const Pokemon::MapSet& maps,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal
){
    const TileGrid& grid = maps.world();
    const auto legs = Pokemon::plan_robust_route(grid, from, goal);
    if (!legs){
        return name + ": no robust route found.";
    }
    std::vector<TilePos> starts = from;
    for (const Pokemon::PlannedLeg& planned : *legs){
        const TileLeg& leg = planned.leg;
        const size_t reach = leg.ends_at_wall ? (size_t)grid.width() + grid.height() : leg.tiles * 2 + 2;
        for (TilePos pos : starts){
            for (size_t i = 0; i < reach; i++){
                const TilePos next = pos.next(leg.direction);
                if (grid.at(next) == TileType::DOOR){
                    return name + ": " + Pokemon::to_string(leg) + " may walk into the door at " + maps.describe(next) + ".";
                }
                const std::optional<TilePos> step = grid.step(pos, leg.direction);
                if (!step){
                    break;
                }
                pos = *step;
            }
        }
        starts = planned.after;
    }
    return "";
}

//  Checks a plan from `from` to `to`: every leg can be walked, ledges are
//  crossed only southward, and each leg's wall flag matches the grid.
//  With `maps`, legs are printed with the map tile they end on, and with
//  the trainers whose sight they cross.
std::string check_plan(
    Logger& logger, const std::string& name,
    const TileGrid& grid, const TilePos& from, const TilePos& to,
    const Pokemon::MapSet* maps = nullptr,
    const std::vector<Pokemon::TrainerSight>& trainers = {}
){
    const std::optional<std::vector<TileLeg>> legs = Pokemon::plan_tile_route(grid, from, to);
    if (!legs){
        return name + ": no route found.";
    }
    auto describe = [&](const TilePos& pos){
        return maps ? maps->describe(pos) : Pokemon::to_string(pos);
    };
    const std::vector<std::vector<std::string>> seen = Pokemon::legs_crossing_sight(grid, from, *legs, trainers);
    std::string text = name + ": from " + describe(from);
    TilePos end = from;
    for (size_t i = 0; i < legs->size(); i++){
        const TileLeg& leg = (*legs)[i];
        end = Pokemon::trace_legs(grid, end, {leg}).back();
        text += "\n    " + Pokemon::to_string(leg) + " to " + describe(end);
        for (const std::string& trainer : seen[i]){
            text += ", crosses " + trainer + "'s sight";
        }
    }
    logger.log(text);

    std::vector<TilePos> tiles;
    try{
        tiles = Pokemon::trace_legs(grid, from, *legs);
    }catch (InternalProgramError& e){
        return name + ": " + e.message();
    }
    if (tiles.back() != to){
        return name + ": the legs end at " + Pokemon::to_string(tiles.back()) + ".";
    }
    for (size_t i = 1; i < tiles.size(); i++){
        const TilePos& a = tiles[i - 1];
        const TilePos& b = tiles[i];
        if (!grid.standable(b)){
            return name + ": the route stands on " + Pokemon::to_string(b) + ".";
        }
        const int dx = b.x - a.x;
        const int dy = b.y - a.y;
        if (std::abs(dx) + std::abs(dy) == 1){
            continue;
        }
        if (dx != 0 || dy != 2 || grid.at(a.next(Direction::SOUTH)) != TileType::LEDGE){
            return name + ": jump from " + Pokemon::to_string(a) + " to " + Pokemon::to_string(b) + " is not a south ledge hop.";
        }
    }
    TilePos pos = from;
    for (const TileLeg& leg : *legs){
        if (leg.tiles == 0){
            return name + ": empty leg.";
        }
        pos = Pokemon::trace_legs(grid, pos, {leg}).back();
        if (leg.ends_at_wall != grid.at_wall(pos, leg.direction)){
            return name + ": leg " + Pokemon::to_string(leg) + " has the wrong wall flag.";
        }
    }
    for (size_t i = 1; i < legs->size(); i++){
        if ((*legs)[i].direction == (*legs)[i - 1].direction){
            return name + ": two legs in a row go " + Pokemon::direction_name((*legs)[i].direction) + ".";
        }
    }
    return "";
}

class Test_TileGridPlans : public UnitTest{
public:
    Test_TileGridPlans()
        : UnitTest("PokemonBDSP::AutoStory - tile grid plans")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const TileGrid& small = small_grid();

        //  South over the ledge is a straight hop; back north must use the gap.
        std::string error = check_plan(logger, "Small south", small, {2, 0}, {2, 4});
        if (!error.empty()){
            return error;
        }
        error = check_plan(logger, "Small north", small, {2, 4}, {2, 0});
        if (!error.empty()){
            return error;
        }
        const auto south = Pokemon::plan_tile_route(small, {2, 0}, {2, 4});
        if (south->size() != 1 || south->front().direction != Direction::SOUTH){
            return "Small south should be one leg over the ledge.";
        }

        //  Walled in by ledges from below.
        const TileGrid pit({
            R"(#.#)",
            R"(#v#)",
            R"(#.#)",
        });
        if (Pokemon::plan_tile_route(pit, {1, 2}, {1, 0})){
            return "A route north over a ledge was found.";
        }

        //  00.04 after the catch: every leg it walks, in order.
        const Pokemon::MapSet& maps = story_maps();
        const std::vector<Pokemon::TrainerSight> trainers = story_trainer_sights();
        const TilePos catch_spot = maps.to_world(MAP_ROUTE_202, Route202::CATCH_SPOT);
        const TilePos jubilife = maps.to_world(MAP_ROUTE_202, Route202::EXIT_JUBILIFE);
        error = check_plan(logger, "Catch spot to Jubilife", maps.world(), catch_spot, jubilife, &maps, trainers);
        if (!error.empty()){
            return error;
        }
        //  00.04's walks in order, from where the step before may leave us.
        const TilePos save_spot = maps.to_world(MAP_ROUTE_202, Route202::SAVE_SPOT);
        const std::vector<TilePos> grass = story_after_training_area();
        const TilePos below_sign = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_SIGN.next(Direction::SOUTH));
        const TilePos below_mart = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR.next(Direction::SOUTH));
        const TilePos row_end = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::FRONT_ROW_EAST_END);
        //  From the trees east of the Mart, 00.04 walks six tiles west, three
        //  off at most, and north (north_through_sign_gap()): from every
        //  column it may reach, north passes no door, and only the gap
        //  between the signs leads out of town.
        std::vector<TilePos> above_sign_gap;
        for (int x = row_end.x - 11; x <= row_end.x - 3; x++){
            const TilePos pos{x, row_end.y};
            const Pokemon::WallHit hit = Pokemon::walk_to_wall(maps.world(), pos, Direction::NORTH);
            if (maps.world().at(hit.wall) == TileType::DOOR){
                return "North from " + maps.describe(pos) + " walks into a door.";
            }
            const std::vector<TilePos> gap = SandgemTown::below_sign_gap();
            const bool in_gap = std::find(gap.begin(), gap.end(), maps.to_local(pos)->pos) != gap.end();
            if (in_gap != (hit.tiles > 2)){
                return "North from " + maps.describe(pos) + " goes " + std::to_string(hit.tiles) + " tiles.";
            }
            if (in_gap){
                above_sign_gap.push_back(hit.stop);
            }
        }
        //  The walks that start or end in the catch grass must stop in it;
        //  the Sandgem walks (`readable`) never need to.
        const struct{ const char* name; std::vector<TilePos> from; std::function<bool(const TilePos&)> goal; bool readable; } walks[] = {
            {"Save spots to the catch grass", story_save_spots(),
                [&](const TilePos& pos){ return std::find(grass.begin(), grass.end(), pos) != grass.end(); }, false},
            {"After training to the Pokemon Center", story_after_training_area(),
                [&](const TilePos& pos){ return pos == below_sign; }, false},
            //  The Mart's door takes a tile either side: enter_lit_door() lines up by sight.
            {"Pokemon Center to the Poke Mart", story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR),
                [&](const TilePos& pos){ return pos.y == below_mart.y && std::abs(pos.x - below_mart.x) <= 1; }, true},
            {"Poke Mart to the trees east of it", story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR),
                [&](const TilePos& pos){ return pos == row_end; }, true},
            {"North of the gap between the signs to the save spot", above_sign_gap,
                [&](const TilePos& pos){ return pos == save_spot; }, true},
            {"Save spots to Jubilife", story_save_spots(), story_at_jubilife_exit, false},
            //  The Sandgem Center test checkpoint: south into town, then to the Center's sign.
            {"Save spot to Sandgem Town", {save_spot},
                [&](const TilePos& pos){ return pos == sandgem_entry_stop(); }, true},
            {"Sandgem's entry to the Pokemon Center", {sandgem_entry_stop()},
                [&](const TilePos& pos){ return pos == below_sign; }, true},
            //  The Mart lady test checkpoint: out of the Center to the tile west
            //  of Route 201's border sign, from the lady back to it, and back.
            //  Those to the sign end with a leg south to the trees in the grass
            //  west of the sign, which cannot be read; reading the sign after it says
            //  where we are, and a tile short is walked east to it.
            {"Pokemon Center to west of the Route 201 sign", story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR),
                [&](const TilePos& pos){ return pos == route201_sign_west(); }, false},
            {"Poke Mart lady to west of the Route 201 sign", {maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_TALK_FROM)},
                [&](const TilePos& pos){ return pos == route201_sign_west(); }, false},
            {"Route 201 sign to the Pokemon Center", {route201_sign_west()},
                [&](const TilePos& pos){ return pos == below_sign; }, true},
            //  The Center walk's re-anchor at the trees east of the Mart.
            {"Trees east of the Mart to the Pokemon Center", {maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::FRONT_ROW_EAST_END)},
                [&](const TilePos& pos){ return pos == below_sign; }, true},
            //  From and back to where the Route 202 checkpoint ends: the doors'
            //  row by the signs' gap.
            {"Below the Center's sign to west of the Route 201 sign", {below_sandgem_center_sign()},
                [&](const TilePos& pos){ return pos == route201_sign_west(); }, false},
            {"Below the Center's sign to the trees east of the Mart", {below_sandgem_center_sign()},
                [&](const TilePos& pos){ return pos == row_end; }, true},
            {"East of the Lab's sign to the trees east of the Mart", {east_of_lab_sign()},
                [&](const TilePos& pos){ return pos == row_end; }, true},
            //  Its re-anchor after a wild battle in the lady's grass.
            {"East end of the lady's row to the Route 201 sign", {route201_row_3_east_end()},
                [&](const TilePos& pos){ return pos == route201_sign_west(); }, false},
        };
        for (const auto& walk : walks){
            error = check_robust_plan(logger, walk.name, maps, walk.from, walk.goal);
            if (!error.empty()){
                return error;
            }
            error = check_plan_avoids_doors(walk.name, maps, walk.from, walk.goal);
            if (!error.empty()){
                return error;
            }
            if (walk.readable){
                error = check_plan_stops_readable(walk.name, maps, walk.from, walk.goal);
                if (!error.empty()){
                    return error;
                }
            }
        }

        //  In town, short open legs are cheap: from where the walk south stops,
        //  the plan is the user's (2026-10-30): west to the wall, a tile east
        //  into the gap's column, straight south through the gap between the
        //  signs, a tile west, then north until the Center's sign stops us
        //  (r134a: "south 5" went 6). Not round the town.
        {
            const auto legs = Pokemon::plan_robust_route(maps.world(), {sandgem_entry_stop()},
                [&](const TilePos& pos){ return pos == below_sign; }, town_plan_costs());
            std::string read;
            for (const Pokemon::PlannedLeg& planned : legs ? *legs : std::vector<Pokemon::PlannedLeg>()){
                read += " [" + Pokemon::to_string(planned.leg) + "]";
            }
            const std::vector<std::pair<Direction, size_t>> expected{
                {Direction::WEST, 9}, {Direction::EAST, 1}, {Direction::SOUTH, 6}, {Direction::WEST, 1}, {Direction::NORTH, 2},
            };
            bool same = legs && legs->size() == expected.size();
            for (size_t i = 0; same && i < expected.size(); i++){
                same = (*legs)[i].leg.direction == expected[i].first && (*legs)[i].leg.tiles == expected[i].second;
            }
            if (!same){
                return "Sandgem's entry to below the Center's sign, town costs: expected west 9, east 1, south 6, west 1, north 2; planned" +
                    (read.empty() ? std::string(" nothing") : read) + ".";
            }
            logger.log("Sandgem's entry to below the Center's sign, town costs:" + read);
        }

        //  The walk south from the save spot keeps out of the tall grass, so
        //  no wild battle can stop it.
        {
            const auto legs = Pokemon::plan_robust_route(maps.world(), {save_spot}, sandgem_entry_stop());
            if (!legs){
                return "Save spot to Sandgem Town: no robust route found.";
            }
            std::vector<TilePos> starts{save_spot};
            for (const Pokemon::PlannedLeg& planned : *legs){
                for (TilePos pos : starts){
                    for (size_t i = 0; i < planned.leg.tiles; i++){
                        const std::optional<TilePos> step = maps.world().step(pos, planned.leg.direction);
                        if (!step){
                            break;
                        }
                        pos = *step;
                        if (maps.world().at(pos) == TileType::TALL_GRASS){
                            return "Save spot to Sandgem Town: " + Pokemon::to_string(planned.leg) +
                                " crosses tall grass at " + maps.describe(pos) + ".";
                        }
                    }
                }
                starts = planned.after;
            }
        }

        //  00.05 in Jubilife: every leg the segment walks, from where the one
        //  before it may have left us.
        {
            using namespace JubilifeCity;
            const Pokemon::MapSet& town = jubilife_maps();
            auto at = [&](const TilePos& pos){ return town.to_world(MAP_JUBILIFE_CITY, pos); };
            std::vector<TilePos> dawn;
            for (const TilePos& pos : up_the_steps()){
                dawn.push_back(at(pos));
            }
            const TilePos school = at(TRAINERS_SCHOOL_DOOR.next(Direction::SOUTH));
            const TilePos center = at(POKEMON_CENTER_DOOR.next(Direction::SOUTH));
            const std::vector<TilePos> out_of_school{school.next(Direction::WEST), school, school.next(Direction::EAST)};
            const TilePos president = at(POKETCH_PRESIDENT).next(Direction::EAST);
            const TilePos clown_mart = at(CLOWN_MART).next(Direction::SOUTH);
            const TilePos clown_tv = at(CLOWN_TV_STATION).next(Direction::EAST);
            const TilePos clown_company = at(CLOWN_POKETCH_COMPANY).next(Direction::EAST);
            //  A door's goal is its front row within a tile of its column; an
            //  NPC's is the one tile beside them.
            const struct{ const char* name; std::vector<TilePos> from; TilePos to; bool door; } legs[] = {
                {"Jubilife: after Dawn to the Trainers' School", dawn, school, true},
                {"Jubilife: school to the president", out_of_school, president, false},
                {"Jubilife: president to the clown by the Mart", {president}, clown_mart, false},
                {"Jubilife: Mart clown to the TV station clown", {clown_mart}, clown_tv, false},
                {"Jubilife: TV station clown to the Poketch Company clown", {clown_tv}, clown_company, false},
                {"Jubilife: Poketch Company clown to the president", {clown_company}, president, false},
                {"Jubilife: president to the Pokemon Center", {president}, center, true},
            };
            std::string failed;
            //  01.03 in the Oreburgh Mine: the corridor's top to below Roark, and back.
            {
                const Pokemon::MapSet& mine = oreburgh_mine_maps();
                auto m = [&](const TilePos& pos){ return mine.to_world(MAP_OREBURGH_MINE, pos); };
                std::vector<TilePos> top;
                for (const TilePos& pos : OreburghMine::corridor_top()){
                    top.push_back(m(pos));
                }
                const TilePos below_roark = m(OreburghMine::ROARK).next(Direction::SOUTH);
                const TilePos roark = m(OreburghMine::ROARK);
                std::string leg_error = check_robust_plan(logger, "Mine: corridor top to the stretch east of Roark", mine, top,
                    [&](const TilePos& pos){ return pos.y >= roark.y && pos.y <= roark.y + 1 && pos.x >= roark.x + 2 && pos.x <= roark.x + 7; });
                failed += leg_error.empty() ? "" : leg_error + " ";
                const TilePos exit = m(OreburghMine::CORRIDOR_EXIT_START);
                leg_error = check_robust_plan(logger, "Mine: below Roark to the corridor's corner", mine, {below_roark},
                    [&](const TilePos& pos){ return pos == exit; });
                failed += leg_error.empty() ? "" : leg_error + " ";
            }
            //  01.05 in the Ravaged Path: from the south door to a rock across the
            //  corridor, and once it is smashed, to the tile above the way out.
            {
                const Pokemon::MapSet cave = ravaged_path_maps();
                for (const TilePos& rock : RavagedPath::rocks()){
                    if (cave.world().at(rock) != Pokemon::TileType::BLOCKED){
                        failed += "Ravaged Path: rock " + cave.describe(rock) + " is not blocked. ";
                    }
                }
                const std::vector<TilePos>& before = RavagedPath::before_rocks();
                std::string leg_error = check_robust_plan(logger, "Ravaged Path: south door to a rock", cave, {RavagedPath::ENTRANCE},
                    [&](const TilePos& pos){ return std::find(before.begin(), before.end(), pos) != before.end(); });
                failed += leg_error.empty() ? "" : leg_error + " ";
                for (const TilePos& from : before){
                    const TilePos rock = from.next(Direction::EAST);
                    const Pokemon::MapSet open = ravaged_path_maps({rock});
                    leg_error = check_robust_plan(logger, "Ravaged Path: after smashing " + open.describe(rock) + ", to the corridor's east end", open, {from},
                        [&](const TilePos& pos){ return pos == RavagedPath::EAST_END; });
                    failed += leg_error.empty() ? "" : leg_error + " ";
                }
                //  What 01.05 plans: from either tile, with both rocks open.
                std::vector<TilePos> both;
                for (const TilePos& from : before){
                    both.push_back(from.next(Direction::EAST));
                }
                leg_error = check_robust_plan(logger, "Ravaged Path: from either rock's tile to the corridor's east end", ravaged_path_maps(both), before,
                    [&](const TilePos& pos){ return pos == RavagedPath::EAST_END; });
                failed += leg_error.empty() ? "" : leg_error + " ";
                if (!(RavagedPath::EAST_END.next(Direction::WEST) == RavagedPath::ABOVE_EXIT) ||
                    cave.world().at(RavagedPath::ABOVE_EXIT.next(Direction::SOUTH)) != Pokemon::TileType::DOOR
                ){
                    failed += "Ravaged Path: the way out is not a door a tile west of the corridor's east end. ";
                }
            }
            //  01.06 on Route 203, then Jubilife's north-east corner: from the
            //  Oreburgh Gate's west exit to its road near Jubilife, and from
            //  Jubilife's east stairs to the foot of its north road.
            {
                std::string leg_error = check_robust_plan(logger, "Route 203: Oreburgh Gate to row 17", route203_maps(),
                    Route203::gate_exit(), on_route203_west_line);
                failed += leg_error.empty() ? "" : leg_error + " ";
                leg_error = check_robust_plan(logger, "Jubilife: east stairs to the north road", jubilife_east_maps(),
                    jubilife_east_entrance(), on_jubilife_north_road, town_plan_costs());
                failed += leg_error.empty() ? "" : leg_error + " ";
            }
            //  01.08 on Route 204's south half: from the top of Jubilife's
            //  street to the tile below the Ravaged Path's door.
            {
                //  Planned as the walker plans it: the sight lines it keeps
                //  out of are off the map.
                Pokemon::TileGrid grid = Route204South::grid();
                for (const TilePos& pos : Route204South::tiles_to_avoid()){
                    grid.set(pos, Pokemon::TileType::UNKNOWN);
                }
                Pokemon::MapSet route;
                route.add(MAP_ROUTE_204_SOUTH, grid, {0, 0});
                const std::string leg_error = check_robust_plan(logger, "Route 204 South: Jubilife to the Ravaged Path door", route,
                    Route204South::jubilife_entry(), [](const TilePos& pos){ return pos == Route204South::BELOW_DOOR; }, town_plan_costs());
                failed += leg_error.empty() ? "" : leg_error + " ";
                for (const Pokemon::TrainerSight& trainer : Route204South::trainer_sights()){
                    if (route.world().at(trainer.pos) != Pokemon::TileType::NPC){
                        failed += "Route 204 South: " + trainer.name + " is not on an NPC tile. ";
                    }
                }
            }
            for (const auto& leg : legs){
                const TilePos to = leg.to;
                const bool door = leg.door;
                const std::string leg_error = check_robust_plan(logger, leg.name, town, leg.from,
                    [&](const TilePos& pos){
                        return door ? pos.y == to.y && pos.x >= to.x - 1 && pos.x <= to.x + 1 : pos == to;
                    }, town_plan_costs());
                failed += leg_error.empty() ? "" : leg_error + " ";
            }
            if (!failed.empty()){
                return failed;
            }
        }
        return true;
    }
};

class Test_IntroDecisions : public UnitTest{
public:
    Test_IntroDecisions()
        : UnitTest("PokemonBDSP::AutoStory - new-game intro decisions")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            const char* screen;
            IntroProgress progress;
            IntroStep expected;
        };
        const IntroProgress start{false, false};
        const IntroProgress named{true, false};
        const IntroProgress done{true, true};
        const Case cases[] = {
            {"user-select",         start,  IntroStep::SELECT_USER},
            {"home",                done,   IntroStep::FAIL_NOT_IN_GAME},
            {"keyboard",            start,  IntroStep::ENTER_PLAYER_NAME},
            {"keyboard",            named,  IntroStep::CANCEL_RIVAL_KEYBOARD},
            {"rival-list-tell",     named,  IntroStep::MOVE_TO_PRESET},
            {"rival-list-preset",   named,  IntroStep::PICK_PRESET},
            {"menu",                done,   IntroStep::CLOSE_MENU_AND_FINISH},
            //  The appearance grid has its prompt in a dialogue box.
            {"dialog",              start,  IntroStep::ADVANCE},
            {"dialog",              done,   IntroStep::ADVANCE},
            //  Language and the appearance preview: no X before the bedroom is possible.
            {"other",               start,  IntroStep::ADVANCE},
            {"other",               named,  IntroStep::ADVANCE},
            {"other",               done,   IntroStep::PROBE_FOR_MENU},
        };
        for (const Case& c : cases){
            const IntroStep actual = decide_intro_step(c.screen, c.progress);
            if (actual != c.expected){
                return std::string("Screen \"") + c.screen + "\" (player named " +
                    (c.progress.player_named ? "yes" : "no") + ", rival named " +
                    (c.progress.rival_named ? "yes" : "no") + "): step " +
                    std::to_string((int)actual) + ", expected " + std::to_string((int)c.expected) + ".";
            }
        }
        if (advance_after_probe(true)){
            return "A after X changed the screen would pick from whatever X opened.";
        }
        if (!advance_after_probe(false)){
            return "A must advance a screen X left unchanged.";
        }
        return true;
    }
};

}


class Test_LocationBanner : public UnitTest{
public:
    Test_LocationBanner()
        : UnitTest("PokemonBDSP::AutoStory - location banner match")
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        struct Case{
            const char* read;
            const char* expected;
            bool match;
        };
        const Case cases[] = {
            {"sandgemtown",     "Sandgem Town",     true},
            {"andgemltown",     "Sandgem Town",     true},     //  r109's OCR
            {"Route 202",       "Route 202",        true},
            {"route2o2",        "Route 202",        false},    //  a digit read as a letter is not trusted
            {"route201",        "Route 202",        false},
            {"jubilifecity",    "Sandgem Town",     false},
            {"twinleaftown",    "Sandgem Town",     false},
            {"",                "Sandgem Town",     false},
        };
        for (const Case& c : cases){
            if (location_banner_matches(c.read, c.expected) != c.match){
                return std::string("Banner \"") + c.read + "\" for " + c.expected +
                    (c.match ? " should match." : " should not match.");
            }
        }
        return true;
    }
};


//  The walk to the Poke Mart may be stopped short by someone in the way, and
//  is then made again from the tiles the doorway on screen puts us on, give
//  01.10 on Route 204's north half: the line a human walked (2026-10-31),
//  corner to corner, from the Ravaged Path's exit into Floaroma. Each
//  stretch must plan as the one straight tap leg the human took, on the
//  grid as it is: a detour here means the grid or the line changed.
class Test_Route204NorthLine : public UnitTest{
public:
    Test_Route204NorthLine()
        : UnitTest("PokemonBDSP::AutoStory - Route 204 North: the line into Floaroma")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const Pokemon::MapSet& route = route204_north_maps();
        const std::vector<TilePos> waypoints = Route204::floaroma_waypoints();
        struct Expected{ Direction direction; size_t tiles; };
        const std::vector<Expected> expected{
            {Direction::SOUTH, 1}, {Direction::WEST, 9}, {Direction::NORTH, 5}, {Direction::EAST, 8},
            {Direction::NORTH, 10}, {Direction::WEST, 8}, {Direction::NORTH, 7},
        };
        if (waypoints.size() != expected.size()){
            return "Route 204 North: " + std::to_string(waypoints.size()) + " waypoints, expected " + std::to_string(expected.size()) + ".";
        }
        std::vector<TilePos> at{Route204::CAVE_EXIT};
        for (size_t i = 0; i < waypoints.size(); i++){
            const TilePos goal = waypoints[i];
            const std::string name = "Route 204 North stretch " + std::to_string(i + 1) + " to " + route.describe(goal);
            const auto plan = Pokemon::plan_robust_route(route.world(), at,
                [&](const TilePos& pos){ return pos == goal; }, waypoint_plan_costs());
            if (!plan){
                return name + ": no plan.";
            }
            std::string text = name + ":";
            for (const Pokemon::PlannedLeg& leg : *plan){
                text += " " + to_string(leg.leg);
            }
            logger.log(text);
            if (plan->size() != 1){
                return name + ": " + std::to_string(plan->size()) + " legs, expected the one straight leg.";
            }
            const Pokemon::TileLeg& leg = (*plan)[0].leg;
            if (leg.ends_at_wall || leg.direction != expected[i].direction || leg.tiles != expected[i].tiles){
                return name + ": planned " + to_string(leg) + ", expected an open leg of " +
                    std::to_string(expected[i].tiles) + " tiles.";
            }
            if ((*plan)[0].after.size() != 1 || !((*plan)[0].after[0] == goal)){
                return name + ": the leg does not end exactly on the waypoint.";
            }
            at = {goal};
        }
        if (!Route204::in_floaroma_path(at[0])){
            return "Route 204 North: the last waypoint is not in the path into Floaroma.";
        }
        for (const Pokemon::TrainerSight& trainer : Route204::north_trainer_sights()){
            if (route.world().at(trainer.pos) != Pokemon::TileType::NPC){
                return "Route 204 North: " + trainer.name + " is not on an NPC tile.";
            }
        }
        return true;
    }
};

//  Every walk a checkpoint makes without a plan is written down as legs
//  (Data/PokemonBDSP_AutoStory_RouteRecords). A record with a start tile on
//  a grid whose rows are the game's is traced leg by leg: counted legs stay
//  on standable tiles, a leg to a wall finds one, a leg to a person or a
//  door ends in front of one. The others are checked for shape and logged
//  as documented, not traced, with why.
class Test_RouteRecords : public UnitTest{
public:
    Test_RouteRecords()
        : UnitTest("PokemonBDSP::AutoStory - route records")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        size_t traced = 0, documented = 0;
        for (const RouteRecords::RouteRecord& record : RouteRecords::all()){
            const std::string name = record.checkpoint + " " + record.title;
            if (record.legs.empty() || record.source.empty() || record.title.empty()){
                return name + ": a record needs legs, a title and a source.";
            }
            for (const RouteRecords::RouteLeg& leg : record.legs){
                if (leg.end == RouteRecords::LegEnd::COUNT && leg.tiles == 0){
                    return name + ": a counted leg of 0 tiles.";
                }
            }
            const TileGrid* grid = record.grid ? record.grid() : nullptr;
            if (!record.traceable || grid == nullptr || !record.start){
                logger.log(name + ": documented, not traced (" +
                    (grid == nullptr ? "no grid" : !record.traceable ? "grid rows not the game's" : "no start tile") + ").");
                documented++;
                continue;
            }
            TilePos pos = *record.start;
            if (!grid->standable(pos)){
                return name + ": the start tile is not standable.";
            }
            for (size_t i = 0; i < record.legs.size(); i++){
                const RouteRecords::RouteLeg& leg = record.legs[i];
                const std::string leg_name = name + " leg " + std::to_string(i + 1);
                switch (leg.end){
                case RouteRecords::LegEnd::COUNT:
                    for (size_t k = 0; k < leg.tiles; k++){
                        pos = pos.next(leg.direction);
                        if (!grid->standable(pos)){
                            return leg_name + ": tile " + std::to_string(k + 1) + " is not standable.";
                        }
                    }
                    break;
                case RouteRecords::LegEnd::WALL:{
                    const Pokemon::WallHit hit = Pokemon::walk_to_wall(*grid, pos, leg.direction);
                    if (hit.tiles == 0){
                        return leg_name + ": already at the wall.";
                    }
                    if (leg.tiles != 0 && hit.tiles != leg.tiles){
                        return leg_name + ": the wall is " + std::to_string(hit.tiles) + " tiles away, the record says " + std::to_string(leg.tiles) + ".";
                    }
                    pos = hit.stop;
                    break;
                }
                case RouteRecords::LegEnd::NPC:
                case RouteRecords::LegEnd::DOOR:{
                    const Pokemon::WallHit hit = Pokemon::walk_to_wall(*grid, pos, leg.direction);
                    pos = hit.stop;
                    const Pokemon::TileType next = grid->at(pos.next(leg.direction));
                    const Pokemon::TileType wanted = leg.end == RouteRecords::LegEnd::NPC ? Pokemon::TileType::NPC : Pokemon::TileType::DOOR;
                    if (next != wanted){
                        return leg_name + ": does not end in front of " + (wanted == Pokemon::TileType::NPC ? "a person." : "a door.");
                    }
                    break;
                }
                case RouteRecords::LegEnd::BANNER:
                case RouteRecords::LegEnd::TRANSITION:
                case RouteRecords::LegEnd::SCENE:
                    //  Leaves the map or the game takes over: nothing to trace.
                    break;
                }
            }
            logger.log(name + ": traced " + std::to_string(record.legs.size()) + " legs.");
            traced++;
        }
        logger.log("Route records: " + std::to_string(traced) + " traced, " + std::to_string(documented) + " documented only.");
        return true;
    }
};

//  or take one each way: from every such block of tiles south of the Mart,
//  a robust plan to the row below its door must exist.
class Test_PokeMartShortWalks : public UnitTest{
public:
    Test_PokeMartShortWalks()
        : UnitTest("PokemonBDSP::AutoStory - walks short of the Poke Mart")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const Pokemon::MapSet& maps = story_maps();
        const TileGrid& grid = maps.world();
        const TilePos front = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR.next(Direction::SOUTH));
        const auto at_front = [&](const TilePos& pos){ return pos.y == front.y && std::abs(pos.x - front.x) <= 1; };
        //  The open ground south of the Mart, to the trees east of town and
        //  the fence rows below.
        for (int y = front.y + 1; y <= front.y + 7; y++){
            for (int x = front.x - 6; x <= front.x + 2; x++){
                std::vector<TilePos> block;
                for (int dy = -1; dy <= 1; dy++){
                    for (int dx = -1; dx <= 1; dx++){
                        if (grid.standable({x + dx, y + dy})){
                            block.push_back({x + dx, y + dy});
                        }
                    }
                }
                if (block.empty()){
                    continue;
                }
                const std::string error = check_robust_plan(logger, "Short of the Poke Mart about " + maps.describe({x, y}),
                    maps, block, at_front);
                if (!error.empty()){
                    return error;
                }
            }
        }
        return true;
    }
};


//  The Mart's doorway on the failure screenshots of r121a and r122a, kept in
//  the unit test resources (the CommandLineTests checkout): where it is
//  relative to the player, who is in the middle of the screen.
class Test_LitDoorwayScreenshots : public UnitTest{
public:
    Test_LitDoorwayScreenshots()
        : UnitTest("PokemonBDSP::AutoStory - lit doorway screenshots")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{
            const char* file;
            double tiles_across_min;
            double tiles_across_max;
            double rows_up_min;
            double rows_up_max;
        };
        const Case cases[] = {
            //  r121a: below-left of the door, which is part of a tile east, straight ahead.
            {"PokemonBDSP/AutoStory/SandgemMartDoorUpRight-20261030-023443.png", 0.2, 0.7, 0.5, 1.5},
            //  r122a: a tile west of the door and three or four rows south of its front.
            {"PokemonBDSP/AutoStory/SandgemMartDoorFarLeft-20261030-024648.png", -1.3, -0.5, 2.5, 4.5},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const ImageRGB32 screen(path);
            const std::optional<LitDoorway> doorway = find_lit_doorway(screen);
            if (!doorway){
                return std::string(c.file) + ": no lit doorway found.";
            }
            logger.log(std::string(c.file) + ": doorway at " + std::to_string(doorway->x) + ", " +
                std::to_string(doorway->y) + ": " + std::to_string(doorway->tiles_across) + " tiles across, " +
                std::to_string(doorway->rows_up) + " rows up.");
            if (doorway->tiles_across < c.tiles_across_min || doorway->tiles_across > c.tiles_across_max){
                return std::string(c.file) + ": doorway " + std::to_string(doorway->tiles_across) +
                    " tiles across, expected " + std::to_string(c.tiles_across_min) + " to " +
                    std::to_string(c.tiles_across_max) + ".";
            }
            if (doorway->rows_up < c.rows_up_min || doorway->rows_up > c.rows_up_max){
                return std::string(c.file) + ": doorway " + std::to_string(doorway->rows_up) +
                    " rows up, expected " + std::to_string(c.rows_up_min) + " to " +
                    std::to_string(c.rows_up_max) + ".";
            }
        }
        return true;
    }
};


//  The save prompt r124a took beside Sandgem's Poke Mart, sure it stood on
//  Route 202 (in CommandLineTests under PokemonBDSP/AutoStory): its card
//  names Sandgem Town, which the save must read before confirming.
class Test_SaveScreenLocation : public UnitTest{
public:
    Test_SaveScreenLocation()
        : UnitTest("PokemonBDSP::AutoStory - save screen location")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string path = UNIT_TEST_RESOURCE_PATH() + "PokemonBDSP/AutoStory/SaveScreenSandgemTown-20261030-034444.png";
        if (!Filesystem::is_regular_file(path)){
            return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
        }
        const std::string read = read_save_screen_location(ImageRGB32(path));
        logger.log("Save screen location read: \"" + read + "\".");
        if (!location_banner_matches(read, MAP_SANDGEM_TOWN)){
            return "Save screen location \"" + read + "\" should match Sandgem Town.";
        }
        if (location_banner_matches(read, MAP_ROUTE_202)){
            return "Save screen location \"" + read + "\" should not match Route 202.";
        }
        return true;
    }
};


//  The Route 202 to Sandgem Center checkpoint's events against a scripted
//  game: where the player is, and what goes wrong on the way.
class FakeSandgemCenter : public SandgemCenterActions{
public:
    enum class Where{ ROUTE_202, SANDGEM_ENTRY, SHORT_OF_DOOR, BELOW_SIGN, DOOR_FRONT, LOST, MART_ROW };

    std::string save_location = "Route 202";
    //  Walks to the Center that end three rows short (someone in the way).
    size_t short_walks = 0;
    //  Walks that a townsperson's talk stops, with the dialogue left open.
    size_t talking_walks = 0;
    //  Walks to the Center that end lost in town (a wall leg short, r127a).
    size_t lost_walks = 0;
    Where where = Where::ROUTE_202;
    bool dialogue = false;
    std::vector<std::string> trace;

    virtual std::string save_screen_location() override{
        trace.push_back("read save screen");
        return save_location;
    }
    virtual Pokemon::Outcome walk(
        std::vector<TilePos>& at, const std::function<bool(const TilePos&)>& goal, const std::string& label
    ) override{
        trace.push_back("walk: " + label);
        if (talking_walks > 0){
            talking_walks--;
            dialogue = true;
            return Pokemon::outcome_failed("walk_failed", "a dialogue no trainer explains");
        }
        if (goal(sandgem_entry_stop())){
            at = {sandgem_entry_stop()};
            where = Where::SANDGEM_ENTRY;
            return Pokemon::outcome_ok();
        }
        if (goal(below_sandgem_center_sign())){
            if (lost_walks > 0){
                lost_walks--;
                where = Where::LOST;
                return Pokemon::outcome_failed("walk_failed", "ran out of time short of the wall");
            }
            at = {below_sandgem_center_sign()};
            where = Where::BELOW_SIGN;
            if (short_walks > 0){
                short_walks--;
                where = Where::SHORT_OF_DOOR;
            }
            return Pokemon::outcome_ok();
        }
        return Pokemon::outcome_failed("no_plan", label);
    }
    virtual bool east_then_south_into_sandgem() override{
        trace.push_back("east, then south");
        const bool entered = where == Where::ROUTE_202;
        where = Where::SANDGEM_ENTRY;
        return entered;
    }
    virtual Pokemon::Outcome walk_in_town(
        std::vector<TilePos>& at, const std::function<bool(const TilePos&)>& goal, const std::string& label
    ) override{
        return walk(at, goal, label);
    }
    virtual DialogueRead read_sign(Direction facing, const std::string& label) override{
        trace.push_back("read: " + label);
        if (where == Where::BELOW_SIGN && facing == Direction::NORTH){
            return DialogueRead{.opened = true, .closed = true, .text = "Pokemon Center"};
        }
        return DialogueRead{};
    }
    virtual std::optional<LitDoorway> lit_doorway() override{
        switch (where){
        case Where::BELOW_SIGN:
            return LitDoorway{.x = 0.3, .y = 0.38, .tiles_across = -3.0, .rows_up = 1.0};
        case Where::SHORT_OF_DOOR:
            return LitDoorway{.x = 0.3, .y = 0.14, .tiles_across = -3.0, .rows_up = 3.0};
        case Where::DOOR_FRONT:
            return LitDoorway{.x = 0.5, .y = 0.38, .tiles_across = 0.1, .rows_up = 1.0};
        case Where::MART_ROW:
            return LitDoorway{.x = 0.37, .y = 0.38, .tiles_across = -2.0, .rows_up = 1.0};
        default:
            return std::nullopt;
        }
    }
    virtual Pokemon::Outcome line_up_with_door() override{
        trace.push_back("line up");
        if (where != Where::BELOW_SIGN){
            return Pokemon::outcome_failed("not_lined_up", "no lit doorway on screen");
        }
        where = Where::DOOR_FRONT;
        return Pokemon::outcome_ok();
    }
    virtual bool inside_pokemon_center() override{
        return false;
    }
    virtual Pokemon::Outcome east_then_north_in_sandgem() override{
        trace.push_back("east, then north");
        where = Where::MART_ROW;
        return Pokemon::outcome_ok();
    }
    virtual bool dialogue_showing() override{
        return dialogue;
    }
    virtual Pokemon::Outcome finish_dialogue() override{
        trace.push_back("finish dialogue");
        dialogue = false;
        return Pokemon::outcome_ok();
    }
};

class Test_SandgemCenterEvents : public UnitTest{
public:
    Test_SandgemCenterEvents()
        : UnitTest("PokemonBDSP::AutoStory - Sandgem Center events")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::string SOUTH = "walk: Route 202 to Sandgem Town";
        const std::string ROW = "walk: To below the Center's sign";
        const std::string READ = "read: the Center's sign";
        struct Case{
            const char* name;
            std::string save_location;
            size_t short_walks;
            size_t talking_walks;
            size_t lost_walks;
            bool completed;
            size_t failed_event;
            std::vector<std::string> trace;
        };
        const Case cases[] = {
            {
                .name = "Straight through",
                .save_location = "Route 202",
                .short_walks = 0,
                .talking_walks = 0,
                .lost_walks = 0,
                .completed = true,
                .failed_event = 0,
                .trace = {"read save screen", SOUTH, ROW, READ},
            },
            {
                .name = "The save is in Sandgem Town",
                .save_location = "Sandgem Town",
                .short_walks = 0,
                .talking_walks = 0,
                .lost_walks = 0,
                .completed = false,
                .failed_event = 1,
                .trace = {"read save screen", "read save screen"},
            },
            {
                .name = "A row off: the sign does not read",
                .save_location = "Route 202",
                .short_walks = 1,
                .talking_walks = 0,
                .lost_walks = 0,
                .completed = false,
                .failed_event = 4,
                .trace = {"read save screen", SOUTH, ROW, READ},
            },
            {
                .name = "A talk stops the walk south",
                .save_location = "Route 202",
                .short_walks = 0,
                .talking_walks = 1,
                .lost_walks = 0,
                .completed = true,
                .failed_event = 0,
                .trace = {"read save screen", SOUTH, "finish dialogue", "east, then south", ROW, READ},
            },
            {
                .name = "The walk in town fails",
                .save_location = "Route 202",
                .short_walks = 0,
                .talking_walks = 0,
                .lost_walks = 1,
                .completed = false,
                .failed_event = 3,
                .trace = {"read save screen", SOUTH, ROW},
            },
        };
        for (const Case& c : cases){
            FakeSandgemCenter game;
            game.save_location = c.save_location;
            game.short_walks = c.short_walks;
            game.talking_walks = c.talking_walks;
            game.lost_walks = c.lost_walks;
            Pokemon::Checkpoint checkpoint = route202_to_sandgem_center_checkpoint(game);
            const std::vector<std::string> problems = Pokemon::lint_checkpoint(checkpoint);
            if (!problems.empty()){
                return std::string(c.name) + ": lint: " + problems.front();
            }
            std::string log_text;
            Pokemon::EventLogger log = [&](const std::string& line){ log_text += "\n    " + line; };
            Pokemon::RunState state;
            const Pokemon::CheckpointReport report = Pokemon::run_checkpoint(state, checkpoint, log);
            logger.log(std::string(c.name) + ":" + log_text);
            if (report.completed != c.completed || report.failed_event != c.failed_event){
                return std::string(c.name) + ": expected " + (c.completed ? "completed" : "failed") +
                    " at event " + std::to_string(c.failed_event) + ", read " + (report.completed ? "completed" : "failed") +
                    " at event " + std::to_string(report.failed_event) + ":" + log_text;
            }
            if (game.trace != c.trace){
                std::string expected, read;
                for (const std::string& step : c.trace){
                    expected += " [" + step + "]";
                }
                for (const std::string& step : game.trace){
                    read += " [" + step + "]";
                }
                return std::string(c.name) + ": expected steps" + expected + ", read" + read + ".";
            }
            if (c.completed && game.where != FakeSandgemCenter::Where::BELOW_SIGN){
                return std::string(c.name) + ": completed, but not on the doors' row.";
            }
        }
        return true;
    }
};


//  The two shots around r126's door walk: below the Center's sign, outside,
//  and on the entrance mat inside, where the lit floor read as a doorway and
//  the walk kept lining up with it (runs 2 and 3).
class Test_PokemonCenterInside : public UnitTest{
public:
    Test_PokemonCenterInside()
        : UnitTest("PokemonBDSP::AutoStory - Pokemon Center inside")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{
            const char* file;
            bool inside;
            bool doorway;
        };
        const Case cases[] = {
            {.file = "PokemonBDSP/AutoStory/SandgemCenterInside-20261030-053555.png", .inside = true, .doorway = false},
            //  r127c: walked in by the turn north at the door, facing north.
            {.file = "PokemonBDSP/AutoStory/SandgemCenterInsideFacingNorth-r127c.png", .inside = true, .doorway = false},
            {.file = "PokemonBDSP/AutoStory/SandgemCenterBelowSign-20261030-053527.png", .inside = false, .doorway = true},
            {.file = "PokemonBDSP/AutoStory/SandgemMartDoorUpRight-20261030-023443.png", .inside = false, .doorway = true},
            {.file = "PokemonBDSP/AutoStory/SandgemMartDoorFarLeft-20261030-024648.png", .inside = false, .doorway = true},
            //  Jubilife (2026-10-30): inside its Center twice, at its door, and
            //  on the doorstep after walking out (the banner up).
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png", .inside = true, .doorway = false},
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c4.png", .inside = true, .doorway = false},
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterFrontDoor-20261030-q2.png", .inside = false, .doorway = true},
            {.file = "PokemonBDSP/AutoStory/JubilifeCenterDoorstep-20261030-c5.png", .inside = false, .doorway = true},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const ImageRGB32 screen(path);
            const bool inside = pokemon_center_inside(screen);
            const std::optional<LitDoorway> doorway = find_lit_doorway(screen);
            logger.log(std::string(c.file) + ": inside " + std::to_string(inside) + ", doorway " +
                (doorway ? std::to_string(doorway->tiles_across) + " across, " + std::to_string(doorway->rows_up) + " rows up" : "none"));
            if (inside != c.inside){
                return std::string(c.file) + ": expected " + (c.inside ? "inside" : "outside") +
                    " a Pokemon Center, read " + (inside ? "inside" : "outside") + ".";
            }
            if (doorway.has_value() != c.doorway){
                return std::string(c.file) + ": expected " + (c.doorway ? "a doorway" : "no doorway") +
                    ", read " + (doorway ? "one" : "none") + ".";
            }
        }
        return true;
    }
};


//  The Poke Mart lady checkpoint against a scripted game: where the player
//  stands along the route measured on the console.
class FakeMartLady : public MartLadyActions{
public:
    enum class Where{
        DOORS_ROW, LAB_EAST, PATH_WEST, GRASS_EDGE, TOP_EDGE, LADY,
        SIGNS_ROW, SHORT_OF_201, ROUTE_201_WEST, PATH_EAST, MART_TREES_LOW, MART_TREES, ROUTE_202,
    };

    //  Walks back east that a wild battle ends short of the sign.
    size_t short_walks = 0;
    //  Talks to the lady before she is there to answer.
    size_t lady_away = 0;
    Where where = Where::DOORS_ROW;
    std::vector<std::string> trace;

    virtual std::string save_screen_location() override{ return ""; }
    virtual Pokemon::Outcome walk(
        std::vector<TilePos>&, const std::function<bool(const TilePos&)>&, const std::string& label
    ) override{
        trace.push_back("walk: " + label);
        return Pokemon::outcome_failed("no_plan", label);
    }
    virtual bool east_then_south_into_sandgem() override{ return false; }
    virtual Pokemon::Outcome walk_in_town(
        std::vector<TilePos>&, const std::function<bool(const TilePos&)>&, const std::string& label
    ) override{
        trace.push_back("walk: " + label);
        return Pokemon::outcome_failed("no_plan", label);
    }
    virtual std::optional<LitDoorway> lit_doorway() override{ return std::nullopt; }
    virtual Pokemon::Outcome line_up_with_door() override{ return Pokemon::outcome_failed("no_plan", "not on this checkpoint"); }
    virtual bool inside_pokemon_center() override{ return false; }
    virtual Pokemon::Outcome east_then_north_in_sandgem() override{ return Pokemon::outcome_ok(); }
    virtual bool dialogue_showing() override{ return false; }
    virtual Pokemon::Outcome finish_dialogue() override{ return Pokemon::outcome_ok(); }
    virtual Pokemon::Outcome walk_until_stopped(Direction direction, size_t, const std::string& label) override{
        trace.push_back("go: " + label);
        const struct{ Where from; Direction direction; Where to; } walks[] = {
            {Where::DOORS_ROW, Direction::WEST, Where::LAB_EAST},
            {Where::PATH_WEST, Direction::WEST, Where::GRASS_EDGE},
            {Where::GRASS_EDGE, Direction::NORTH, Where::TOP_EDGE},
            {Where::LADY, Direction::WEST, Where::TOP_EDGE},
            {Where::TOP_EDGE, Direction::WEST, Where::TOP_EDGE},
            {Where::SIGNS_ROW, Direction::EAST, Where::ROUTE_201_WEST},
            {Where::SHORT_OF_201, Direction::EAST, Where::ROUTE_201_WEST},
            {Where::PATH_EAST, Direction::EAST, Where::MART_TREES_LOW},
            {Where::MART_TREES_LOW, Direction::NORTH, Where::MART_TREES},
        };
        for (const auto& walk : walks){
            if (walk.from == where && walk.direction == direction){
                where = walk.to;
                if (where == Where::ROUTE_201_WEST && short_walks > 0 && walk.from == Where::SIGNS_ROW){
                    short_walks--;
                    where = Where::SHORT_OF_201;
                }
                break;
            }
        }
        return Pokemon::outcome_ok();
    }
    virtual Pokemon::Outcome step(Direction direction, const std::string& label) override{
        trace.push_back("step: " + label);
        if (direction != Direction::SOUTH){
            return Pokemon::outcome_ok();
        }
        if (where == Where::LAB_EAST){
            where = Where::PATH_WEST;
        }else if (where == Where::LADY || where == Where::TOP_EDGE){
            where = Where::SIGNS_ROW;
        }else if (where == Where::ROUTE_201_WEST){
            where = Where::PATH_EAST;
        }
        return Pokemon::outcome_ok();
    }
    virtual DialogueRead talk(Direction, Button, const std::string& label) override{
        trace.push_back("talk: " + label);
        return DialogueRead{};
    }
    virtual DialogueRead read_sign(Direction facing, const std::string& label) override{
        trace.push_back("read: " + label);
        if (where == Where::LAB_EAST && facing == Direction::WEST){
            return DialogueRead{.opened = true, .closed = true, .text = "Pokemon Research Lab"};
        }
        if (where == Where::ROUTE_201_WEST && facing == Direction::EAST){
            return DialogueRead{.opened = true, .closed = true, .text = "Route 201 Sandgem Town"};
        }
        return DialogueRead{};
    }
    virtual DialogueRead talk_walking(Direction direction, size_t, const std::string& label) override{
        trace.push_back("talk: " + label);
        if (direction != Direction::EAST || (where != Where::TOP_EDGE && where != Where::LADY)){
            return DialogueRead{};
        }
        where = Where::LADY;
        if (lady_away > 0){
            lady_away--;
            return DialogueRead{};
        }
        return DialogueRead{.opened = true, .closed = true, .text = "Did you know"};
    }
    virtual Pokemon::Outcome walk_to_route_202_save_spot() override{
        trace.push_back("to Route 202");
        if (where != Where::MART_TREES){
            return Pokemon::outcome_failed("wrong_place", "to Route 202");
        }
        where = Where::ROUTE_202;
        return Pokemon::outcome_ok();
    }
    virtual Pokemon::Outcome save_on_route_202() override{
        trace.push_back("save");
        return where == Where::ROUTE_202 ? Pokemon::outcome_ok() : Pokemon::outcome_failed("wrong_place", "save");
    }
};

class Test_MartLadyEvents : public UnitTest{
public:
    Test_MartLadyEvents()
        : UnitTest("PokemonBDSP::AutoStory - Mart lady events")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const std::vector<std::string> TO_LADY = {"go: West to the Lab's sign", "read: the Lab's sign",
            "step: South onto the path", "go: West along the path", "go: North to the trees"};
        const std::vector<std::string> LADY = {"talk: Poke Mart lady"};
        const std::vector<std::string> RETRY = {"go: West to the grass's edge"};
        const std::vector<std::string> TO_SIGN = {"step: South onto the signs' row", "go: East to the Route 201 sign"};
        const std::vector<std::string> SIGN = {"read: Route 201's sign"};
        const std::vector<std::string> HOME = {"step: South onto the path", "go: East along the path",
            "go: North to the front row", "to Route 202", "save"};
        const auto join = [](std::initializer_list<std::vector<std::string>> parts){
            std::vector<std::string> all;
            for (const std::vector<std::string>& part : parts){
                all.insert(all.end(), part.begin(), part.end());
            }
            return all;
        };
        struct Case{
            const char* name;
            size_t short_walks;
            size_t lady_away;
            bool completed;
            size_t failed_event;
            std::vector<std::string> trace;
        };
        const Case cases[] = {
            {
                .name = "Straight through",
                .short_walks = 0,
                .lady_away = 0,
                .completed = true,
                .failed_event = 0,
                .trace = join({TO_LADY, LADY, TO_SIGN, SIGN, HOME}),
            },
            {
                .name = "Back east, a tile short of the sign",
                .short_walks = 1,
                .lady_away = 0,
                .completed = true,
                .failed_event = 0,
                .trace = join({TO_LADY, LADY, TO_SIGN, SIGN, {"go: On to the sign"}, SIGN, HOME}),
            },
            {
                .name = "The lady answers the second time",
                .short_walks = 0,
                .lady_away = 1,
                .completed = true,
                .failed_event = 0,
                .trace = join({TO_LADY, LADY, RETRY, LADY, TO_SIGN, SIGN, HOME}),
            },
            {
                .name = "The lady never answers",
                .short_walks = 0,
                .lady_away = 5,
                .completed = false,
                .failed_event = 6,
                .trace = join({TO_LADY, LADY, RETRY, LADY, RETRY, LADY}),
            },
        };
        for (const Case& c : cases){
            FakeMartLady game;
            game.short_walks = c.short_walks;
            game.lady_away = c.lady_away;
            Pokemon::Checkpoint checkpoint = sandgem_mart_lady_checkpoint(game);
            const std::vector<std::string> problems = Pokemon::lint_checkpoint(checkpoint);
            if (!problems.empty()){
                return std::string(c.name) + ": lint: " + problems.front();
            }
            std::string log_text;
            Pokemon::EventLogger log = [&](const std::string& line){ log_text += "\n    " + line; };
            Pokemon::RunState state;
            const Pokemon::CheckpointReport report = Pokemon::run_checkpoint(state, checkpoint, log);
            logger.log(std::string(c.name) + ":" + log_text);
            if (report.completed != c.completed || report.failed_event != c.failed_event){
                return std::string(c.name) + ": expected " + (c.completed ? "completed" : "failed") +
                    " at event " + std::to_string(c.failed_event) + ", read " + (report.completed ? "completed" : "failed") +
                    " at event " + std::to_string(report.failed_event) + ":" + log_text;
            }
            if (game.trace != c.trace){
                std::string expected, read;
                for (const std::string& step : c.trace){
                    expected += " [" + step + "]";
                }
                for (const std::string& step : game.trace){
                    read += " [" + step + "]";
                }
                return std::string(c.name) + ": expected steps" + expected + ", read" + read + ".";
            }
        }
        return true;
    }
};


//  00.05a and 00.05b against a scripted game: the door is `door_at` tiles
//  west of the Center's doorstep.
class FakeJubilife : public JubilifeActions{
public:
    size_t door_at = 10;
    bool stuck_on_house = false;
    bool heal_stays_inside = false;
    bool inside = false;
    size_t column = 0;          //  tiles west of the doorstep
    bool on_doorstep = false;
    bool in_school = false;
    std::vector<std::string> trace;

    virtual std::string save_screen_location() override{ return ""; }
    virtual Pokemon::Outcome walk(std::vector<TilePos>&, const std::function<bool(const TilePos&)>&, const std::string& label) override{
        return Pokemon::outcome_failed("no_plan", label);
    }
    virtual bool east_then_south_into_sandgem() override{ return false; }
    virtual Pokemon::Outcome walk_in_town(std::vector<TilePos>&, const std::function<bool(const TilePos&)>&, const std::string& label) override{
        return Pokemon::outcome_failed("no_plan", label);
    }
    //  The Poketch Company's sign reads after `sign_reads_after` tries.
    size_t sign_reads_after = 1;
    size_t sign_tries = 0;
    virtual DialogueRead read_sign(Direction, const std::string& label) override{
        trace.push_back("sign: " + label);
        if (label == "the Poketch Company's sign" && ++sign_tries >= sign_reads_after){
            return DialogueRead{.opened = true, .closed = true, .text = "The Poketch Company"};
        }
        return {};
    }
    virtual std::optional<LitDoorway> lit_doorway() override{ return std::nullopt; }
    virtual Pokemon::Outcome line_up_with_door() override{ return Pokemon::outcome_ok(); }
    virtual bool inside_pokemon_center() override{ return inside; }
    virtual bool indoors() override{ return inside; }
    virtual Pokemon::Outcome hold(Direction, int64_t, const std::string& label) override{
        trace.push_back("hold: " + label);
        return Pokemon::outcome_ok();
    }
    bool president_talks = true;
    virtual DialogueRead walk_until_talked_to(size_t, const std::string& label) override{
        trace.push_back("meet: " + label);
        return president_talks ? DialogueRead{.opened = true, .closed = true, .text = "Oh?"} : DialogueRead{};
    }
    virtual Pokemon::Outcome east_then_north_in_sandgem() override{ return Pokemon::outcome_ok(); }
    virtual bool dialogue_showing() override{ return false; }
    virtual Pokemon::Outcome finish_dialogue() override{ return Pokemon::outcome_ok(); }
    virtual Pokemon::Outcome walk_until_stopped(Direction, size_t, const std::string& label) override{
        trace.push_back("go: " + label);
        return stuck_on_house && label == "East to the house"
            ? Pokemon::outcome_failed("walk_failed", label)
            : Pokemon::outcome_ok();
    }
    virtual DialogueRead talk(Direction, Button, const std::string& label) override{
        trace.push_back("talk: " + label);
        if (label == "Clown by the Poke Mart"){
            return DialogueRead{.opened = true, .closed = true, .text = "Hi!"};
        }
        if (label == "Barry" && barry_answers){
            return DialogueRead{.opened = true, .closed = true, .text = "Barry"};
        }
        if (label == "Clown by the Poketch Company" && ++clown_asks >= clown_answers_at){
            return DialogueRead{.opened = true, .closed = true, .text = "Hi!"};
        }
        return {};
    }
    virtual Pokemon::Outcome step(Direction direction, const std::string& label) override{
        trace.push_back("step: " + label);
        (void)direction;
        return Pokemon::outcome_ok();
    }
    //  The clown answers the `clown_answers_at`th ask.
    size_t clown_answers_at = 7;
    size_t clown_asks = 0;
    virtual DialogueRead talk_walking(Direction, size_t, const std::string& label) override{
        trace.push_back("talk walking: " + label);
        return {};
    }
    virtual Pokemon::Outcome walk_to_route_202_save_spot() override{ return Pokemon::outcome_ok(); }
    virtual Pokemon::Outcome save_on_route_202() override{ return Pokemon::outcome_ok(); }
    virtual Pokemon::Outcome climb_the_steps() override{
        trace.push_back("climb");
        return Pokemon::outcome_ok();
    }
    virtual Pokemon::Outcome heal_at_center() override{
        trace.push_back("heal");
        inside = heal_stays_inside;
        on_doorstep = true;
        column = 0;
        return Pokemon::outcome_ok();
    }
    virtual void turn(Direction direction) override{
        trace.push_back(std::string("turn ") + Pokemon::direction_name(direction));
    }
    //  The lamp: off by `lamp_off` tiles after the counted walk, or unlit.
    std::optional<double> lamp_off;
    virtual std::optional<double> school_door_across() override{
        if (!lamp_off){
            return std::nullopt;
        }
        return (double)door_at - (double)column;
    }
    virtual void step_across(double tiles, const std::string&) override{
        column += (size_t)std::lround(tiles);
    }
    bool barry_answers = true;
    bool left_school = false;
    virtual Pokemon::Outcome leave_south(const std::string&) override{
        left_school = true;
        return Pokemon::outcome_ok();
    }
    //  Half tiles: a nudge east then west lands back on the column.
    int half = 0;
    virtual void nudge(Direction direction, const std::string&) override{
        half += direction == Direction::EAST ? -1 : 1;
        if (half == 2){ column++; half = 0; }
        if (half == -2){ column--; half = 0; }
    }
    virtual Pokemon::Outcome tile(Direction direction, const std::string&) override{
        if (direction == Direction::WEST){
            column++;
        }
        return Pokemon::outcome_ok();
    }
    virtual void close_panel(const std::string& label) override{
        trace.push_back("close: " + label);
    }
    //  The president answers the `president_answers_at`th ask ahead.
    size_t president_answers_at = 5;
    size_t president_asks = 0;
    virtual DialogueRead talk_ahead(Button, const std::string& label) override{
        trace.push_back("ask: " + label);
        if (label == "The Poketch Company president" && ++president_asks >= president_answers_at){
            return DialogueRead{.opened = true, .closed = true, .text = "Bravo!"};
        }
        return {};
    }
    virtual Pokemon::Outcome taps(Direction direction, size_t tiles, const std::string& label) override{
        trace.push_back("taps: " + label);
        if (direction == Direction::WEST){
            column += tiles;
        }
        return Pokemon::outcome_ok();
    }
    virtual bool through_door_north(const std::string&) override{
        trace.push_back("north at " + std::to_string(column));
        in_school = column == door_at;
        return in_school;
    }
};

class Test_JubilifeEvents : public UnitTest{
public:
    Test_JubilifeEvents()
        : UnitTest("PokemonBDSP::AutoStory - Jubilife 00.05a/b events")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{ const char* name; size_t door_at; bool stuck; bool stays_inside; bool center_done; bool school_done; size_t tries; bool lamp = false; };
        const Case cases[] = {
            {"Door eleven tiles west, as measured", 11, false, false, true, true, 1},
            {"Door a whole tile off: a map error, not found", 12, false, false, true, false, 4},
            {"Door off by two: not found", 14, false, false, true, false, 4},
            {"The house never stops us", 11, true, false, false, false, 0},
            {"The heal leaves us inside (r143a)", 11, false, true, false, false, 0},
            {"A tile off, the lamp lit: it lines us up", 12, false, false, true, true, 1, true},
            {"Two tiles off, the lamp lit: it lines us up", 9, false, false, true, true, 1, true},
        };
        for (const Case& c : cases){
            FakeJubilife game;
            game.door_at = c.door_at;
            game.stuck_on_house = c.stuck;
            game.heal_stays_inside = c.stays_inside;
            if (c.lamp){
                game.lamp_off = 0.0;
            }
            for (size_t which = 0; which < 2; which++){
                Pokemon::Checkpoint checkpoint = which == 0 ? jubilife_center_checkpoint(game) : jubilife_school_checkpoint(game);
                const std::vector<std::string> problems = Pokemon::lint_checkpoint(checkpoint);
                if (!problems.empty()){
                    return std::string(c.name) + ": lint: " + problems.front();
                }
            }
            Pokemon::EventLogger log = [&](const std::string& line){ logger.log(line); };
            Pokemon::RunState center_state;
            Pokemon::Checkpoint center = jubilife_center_checkpoint(game);
            const bool center_done = Pokemon::run_checkpoint(center_state, center, log).completed;
            bool school_done = false;
            if (center_done){
                Pokemon::RunState school_state;
                Pokemon::Checkpoint school = jubilife_school_checkpoint(game);
                school_done = Pokemon::run_checkpoint(school_state, school, log).completed;
            }
            if (school_done){
                Pokemon::RunState barry_state;
                Pokemon::Checkpoint barry = jubilife_barry_checkpoint(game);
                const std::vector<std::string> problems = Pokemon::lint_checkpoint(barry);
                if (!problems.empty()){
                    return std::string(c.name) + ": 00.05c lint: " + problems.front();
                }
                if (!Pokemon::run_checkpoint(barry_state, barry, log).completed || !game.left_school){
                    return std::string(c.name) + ": 00.05c did not complete.";
                }
            }
            if (school_done){
                Pokemon::RunState president_state;
                Pokemon::Checkpoint president = jubilife_president_checkpoint(game);
                const std::vector<std::string> problems = Pokemon::lint_checkpoint(president);
                if (!problems.empty()){
                    return std::string(c.name) + ": 00.05d lint: " + problems.front();
                }
                if (!Pokemon::run_checkpoint(president_state, president, log).completed){
                    return std::string(c.name) + ": 00.05d did not complete.";
                }
            }
            if (school_done){
                Pokemon::RunState clown_state;
                Pokemon::Checkpoint clown = jubilife_clown_mart_checkpoint(game);
                const std::vector<std::string> problems = Pokemon::lint_checkpoint(clown);
                if (!problems.empty()){
                    return std::string(c.name) + ": 00.05e lint: " + problems.front();
                }
                if (!Pokemon::run_checkpoint(clown_state, clown, log).completed){
                    return std::string(c.name) + ": 00.05e did not complete.";
                }
            }
            if (school_done){
                Pokemon::RunState clown_state;
                Pokemon::Checkpoint clown = jubilife_clown_company_checkpoint(game);
                const std::vector<std::string> problems = Pokemon::lint_checkpoint(clown);
                if (!problems.empty()){
                    return std::string(c.name) + ": 00.05f lint: " + problems.front();
                }
                if (!Pokemon::run_checkpoint(clown_state, clown, log).completed){
                    return std::string(c.name) + ": 00.05f did not complete.";
                }
            }
            if (school_done){
                Pokemon::RunState poketch_state;
                Pokemon::Checkpoint poketch = jubilife_poketch_checkpoint(game);
                const std::vector<std::string> problems = Pokemon::lint_checkpoint(poketch);
                if (!problems.empty()){
                    return std::string(c.name) + ": 00.05g lint: " + problems.front();
                }
                if (!Pokemon::run_checkpoint(poketch_state, poketch, log).completed){
                    return std::string(c.name) + ": 00.05g did not complete.";
                }
            }
            size_t tries = 0;
            for (const std::string& step : game.trace){
                tries += step.rfind("north at", 0) == 0;
            }
            if (center_done != c.center_done || school_done != c.school_done || (school_done && tries != c.tries)){
                return std::string(c.name) + ": center " + (center_done ? "done" : "failed") +
                    ", school " + (school_done ? "done" : "failed") + ", " + std::to_string(tries) + " tries north.";
            }
        }
        return true;
    }
};


//  00.05f: a tile at a time along the hedges asking north, then waiting in
//  the gap beside the sign until the wandering clown steps next to us.
class Test_JubilifeClownCompany : public UnitTest{
public:
    Test_JubilifeClownCompany()
        : UnitTest("PokemonBDSP::AutoStory - Jubilife 00.05f clown wait")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{ const char* name; size_t sign_after; size_t clown_at; bool completed; };
        const Case cases[] = {
            {"Clown north of the hedge walk", 1, 2, true},
            {"Sign at the first tile, clown comes by in the gap", 1, 5, true},
            {"Sign at the third tile, clown comes by late", 3, 40, true},
            {"Sign never reads: never in the gap", 100, 40, false},
            {"Clown never answers", 1, 1000, false},
        };
        for (const Case& c : cases){
            FakeJubilife game;
            game.sign_reads_after = c.sign_after;
            game.clown_answers_at = c.clown_at;
            Pokemon::EventLogger log = [&](const std::string& line){ logger.log(line); };
            Pokemon::RunState state;
            Pokemon::Checkpoint checkpoint = jubilife_clown_company_checkpoint(game);
            const std::vector<std::string> problems = Pokemon::lint_checkpoint(checkpoint);
            if (!problems.empty()){
                return std::string(c.name) + ": lint: " + problems.front();
            }
            const bool completed = Pokemon::run_checkpoint(state, checkpoint, log).completed;
            if (completed != c.completed){
                return std::string(c.name) + (completed ? ": completed" : ": did not complete") + ".";
            }
        }
        return true;
    }
};


//  The School's lamp at night, from its doorsteps' row (2026-10-30): where
//  the door took us in, the lamp reads within half a tile of the door; a
//  tile either side, more.
class Test_SchoolLamp : public UnitTest{
public:
    Test_SchoolLamp()
        : UnitTest("PokemonBDSP::AutoStory - Jubilife School lamp")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const ImageFloatBox box(0.55, 0.20, 0.30, 0.10);
        struct Case{ const char* file; double min; double max; };
        const Case cases[] = {
            {"PokemonBDSP/AutoStory/JubilifeSchoolRightOfDoor-20261030.png", -1.5, -0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolOneTileShort-20261030.png", -1.5, -0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolAtDoor-20261030.png", -0.5, 0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolAtDoor2-20261030.png", -0.5, 0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", -0.5, 0.5},
            {"PokemonBDSP/AutoStory/JubilifeSchoolOneTilePast-20261030.png", 0.5, 1.5},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const ImageRGB32 screen(path);
            const std::optional<double> lamp = find_lit_lamp_x(screen, box);
            if (!lamp){
                return std::string(c.file) + ": no lit lamp found.";
            }
            const double across = tiles_across_at(*lamp) - tiles_across_at(0.67);
            logger.log(std::string(c.file) + ": lamp at " + std::to_string(*lamp) + ", door " + std::to_string(across) + " tiles across.");
            if (across < c.min || across > c.max){
                return std::string(c.file) + ": door " + std::to_string(across) + " tiles across, expected " +
                    std::to_string(c.min) + " to " + std::to_string(c.max) + ".";
            }
        }
        return true;
    }
};


//  Out of a building, the town's location banner shows; inside, none
//  (Jubilife, 2026-10-30): what says a walk out of a door got out.
//  Indoors or out, wherever the camera is (2026-10-30 screenshots).
class Test_Indoors : public UnitTest{
public:
    Test_Indoors()
        : UnitTest("PokemonBDSP::AutoStory - indoors")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{ const char* file; bool inside; };
        const Case cases[] = {
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c4.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterInsideUpper-20261030-r151a.png", true},
            {"PokemonBDSP/AutoStory/SandgemCenterInside-20261030-053555.png", true},
            {"PokemonBDSP/AutoStory/JubilifeSchoolInside-20261030.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterDoorstep-20261030-c5.png", false},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", false},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep2-20261030.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterFrontDoor-20261030-q2.png", false},
            {"PokemonBDSP/AutoStory/SandgemCenterBelowSign-20261030-053527.png", false},
            {"PokemonBDSP/AutoStory/SandgemMartDoorUpRight-20261030-023443.png", false},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const bool inside = indoors(ImageRGB32(path));
            logger.log(std::string(c.file) + ": " + (inside ? "indoors" : "outside"));
            if (inside != c.inside){
                return std::string(c.file) + ": expected " + (c.inside ? "indoors" : "outside") + ".";
            }
        }
        return true;
    }
};


class Test_LocationBannerShowing : public UnitTest{
public:
    Test_LocationBannerShowing()
        : UnitTest("PokemonBDSP::AutoStory - location banner showing")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        struct Case{ const char* file; bool banner; };
        const Case cases[] = {
            {"PokemonBDSP/AutoStory/JubilifeCenterDoorstep-20261030-c5.png", true},
            {"PokemonBDSP/AutoStory/JubilifeSchoolDoorstep-20261030.png", true},
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c6.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterInside-20261030-c4.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterInsideUpper-20261030-r151a.png", false},
            {"PokemonBDSP/AutoStory/JubilifeCenterFrontDoor-20261030-q2.png", false},
        };
        for (const Case& c : cases){
            const std::string path = UNIT_TEST_RESOURCE_PATH() + c.file;
            if (!Filesystem::is_regular_file(path)){
                return UnitTestResult(UnitTestResult::SKIPPED, "Screenshot not found: " + path);
            }
            const bool banner = location_banner_showing(ImageRGB32(path));
            logger.log(std::string(c.file) + ": banner " + (banner ? "shows" : "none"));
            if (banner != c.banner){
                return std::string(c.file) + ": expected " + (c.banner ? "a banner" : "no banner") + ".";
            }
        }
        return true;
    }
};


void add_tests_AutoStory(UnitTestDatabase& database){
    database.add<Test_IntroDecisions>();
    database.add<Test_TileGridParse>();
    database.add<Test_TileGridWallHit>();
    database.add<Test_TileGridPlans>();
    database.add<Test_PokeMartShortWalks>();
    database.add<Test_Route204NorthLine>();
    database.add<Test_RouteRecords>();
    database.add<Test_LocationBanner>();
    database.add<Test_LitDoorwayScreenshots>();
    database.add<Test_SaveScreenLocation>();
    database.add<Test_SandgemCenterEvents>();
    database.add<Test_PokemonCenterInside>();
    database.add<Test_MartLadyEvents>();
    database.add<Test_JubilifeEvents>();
    database.add<Test_JubilifeClownCompany>();
    database.add<Test_SchoolLamp>();
    database.add<Test_LocationBannerShowing>();
    database.add<Test_Indoors>();
}


}
}
}
