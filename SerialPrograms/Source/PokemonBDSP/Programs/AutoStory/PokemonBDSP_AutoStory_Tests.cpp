/*  BDSP AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <functional>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_JubilifeCity.h"
#include "Data/PokemonBDSP_AutoStory_OreburghMine.h"
#include "Data/PokemonBDSP_AutoStory_RavagedPath.h"
#include "Data/PokemonBDSP_AutoStory_Route204.h"
#include "Data/PokemonBDSP_AutoStory_Route201.h"
#include "Data/PokemonBDSP_AutoStory_Route202.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Utils/PokemonBDSP_AutoStory_NewGame.h"
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
            {MAP_SANDGEM_TOWN,  SandgemTown::grid(),    34, 23},
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
            {MAP_ROUTE_201,     Route201::POTION_GIVER,             TileType::NPC},
            {MAP_ROUTE_201,     Route201::POTION_GIVER.next(Direction::SOUTH), TileType::TALL_GRASS},
            {MAP_ROUTE_201,     Route201::POTION_GIVER_TALK_FROM,   TileType::TALL_GRASS},
            {MAP_ROUTE_201,     Route201::POTION_GIVER_ROW_EAST,    TileType::WALKABLE},
            {MAP_ROUTE_201,     Route201::POTION_GIVER_ROW_WEST_END, TileType::TALL_GRASS},
            {MAP_ROUTE_201,     Route201::POTION_GIVER_ROW_EAST_END, TileType::TALL_GRASS},
            {MAP_SANDGEM_TOWN,  SandgemTown::LAB_DOOR,              TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKEMON_CENTER_DOOR,   TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKE_MART_DOOR,        TileType::DOOR},
            {MAP_ROUTE_202,     Route202::CATCH_SPOT,               TileType::TALL_GRASS},
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
        if (Route201::POTION_GIVER_ROW_EAST.next(Direction::WEST) != Route201::POTION_GIVER_ROW_EAST_END){
            return "The Potion giver's row does not end next to its east tile.";
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
        for (const Case& c : cases){
            const Pokemon::WallHit hit = Pokemon::walk_to_wall(c.grid, c.from, c.direction);
            if (hit.stop != c.stop || hit.tiles != c.tiles || hit.anchored != c.anchored){
                return "Walking " + std::string(Pokemon::direction_name(c.direction)) + " from " +
                    Pokemon::to_string(c.from) + " stops at " + Pokemon::to_string(hit.stop) + " after " +
                    std::to_string(hit.tiles) + " tiles" + (hit.anchored ? "" : " (not anchored)") +
                    ", expected " + Pokemon::to_string(c.stop) + " after " + std::to_string(c.tiles) + ".";
            }
        }
        return true;
    }
};

//  Plans from `from` to `goal` allowing open legs a tile short or long, and
//  logs the legs with every map tile each may end on.
std::string check_robust_plan(
    Logger& logger, const std::string& name, const Pokemon::MapSet& maps,
    const std::vector<TilePos>& from, const std::function<bool(const TilePos&)>& goal
){
    const auto legs = Pokemon::plan_robust_route(maps.world(), from, goal);
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

        const Pokemon::MapSet& maps = story_maps();
        const std::vector<Pokemon::TrainerSight> trainers = story_trainer_sights();
        const TilePos catch_spot = maps.to_world(MAP_ROUTE_202, Route202::CATCH_SPOT);
        const TilePos potion = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_ROW_EAST);
        const TilePos jubilife = maps.to_world(MAP_ROUTE_202, Route202::EXIT_JUBILIFE);
        error = check_plan(logger, "Catch spot to Potion giver", maps.world(), catch_spot, potion, &maps, trainers);
        if (!error.empty()){
            return error;
        }
        error = check_plan(logger, "Potion giver to Jubilife", maps.world(), potion, jubilife, &maps, trainers);
        if (!error.empty()){
            return error;
        }
        const TilePos below_sign = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_SIGN.next(Direction::SOUTH));
        error = check_robust_plan(logger, "After training to the Pokemon Center", maps, story_after_training_area(),
            [&](const TilePos& pos){ return pos == below_sign; });
        if (!error.empty()){
            return error;
        }
        const TilePos below_door = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR.next(Direction::SOUTH));
        error = check_robust_plan(logger, "Pokemon Center to Jubilife", maps, {below_door}, story_at_jubilife_exit);
        if (!error.empty()){
            return error;
        }
        error = check_robust_plan(logger, "After training to Jubilife", maps, story_after_training_area(), story_at_jubilife_exit);
        if (!error.empty()){
            return error;
        }
        error = check_robust_plan(logger, "Catch spot to Potion giver", maps, {catch_spot},
            [&](const TilePos& pos){ return pos == potion; });
        if (!error.empty()){
            return error;
        }
        //  To where she is talked to, and back to the catch grass to save.
        const TilePos talk_from = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_TALK_FROM);
        error = check_robust_plan(logger, "Catch spot to the Potion giver", maps, {catch_spot},
            [&](const TilePos& pos){ return pos == talk_from; });
        if (!error.empty()){
            return error;
        }
        const TilePos save_spot = maps.to_world(MAP_ROUTE_202, Route202::POTION_SAVE_SPOT);
        error = check_robust_plan(logger, "Potion giver to the save spot", maps, {talk_from},
            [&](const TilePos& pos){ return pos == save_spot; });
        if (!error.empty()){
            return error;
        }
        const std::vector<TilePos> grass = story_after_training_area();
        error = check_robust_plan(logger, "Save spot to the catch grass", maps, {save_spot},
            [&](const TilePos& pos){ return std::find(grass.begin(), grass.end(), pos) != grass.end(); });
        if (!error.empty()){
            return error;
        }

        //  00.05 in Jubilife: every leg the segment walks, from where the one
        //  before it may have left us.
        {
            using namespace JubilifeCity;
            const Pokemon::MapSet& town = jubilife_maps();
            auto at = [&](const TilePos& pos){ return town.to_world(MAP_JUBILIFE_CITY, pos); };
            std::vector<TilePos> dawn;
            for (const TilePos& pos : after_dawn()){
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
            //  01.05 on Route 204's north half: from the Ravaged Path's door to
            //  the path between the flower beds into Floaroma.
            {
                const Pokemon::MapSet& route = route204_north_maps();
                const std::string leg_error = check_robust_plan(logger, "Route 204: Ravaged Path to the Floaroma path", route, {Route204::CAVE_EXIT},
                    Route204::in_floaroma_path);
                failed += leg_error.empty() ? "" : leg_error + " ";
                for (const Pokemon::TrainerSight& twin : Route204::north_trainer_sights()){
                    if (route.world().at(twin.pos) != Pokemon::TileType::NPC){
                        failed += "Route 204: " + twin.name + " is not on an NPC tile. ";
                    }
                }
            }
            for (const auto& leg : legs){
                const TilePos to = leg.to;
                const bool door = leg.door;
                const std::string leg_error = check_robust_plan(logger, leg.name, town, leg.from,
                    [&](const TilePos& pos){
                        return door ? pos.y == to.y && pos.x >= to.x - 1 && pos.x <= to.x + 1 : pos == to;
                    });
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


void add_tests_AutoStory(UnitTestDatabase& database){
    database.add<Test_IntroDecisions>();
    database.add<Test_TileGridParse>();
    database.add<Test_TileGridWallHit>();
    database.add<Test_TileGridPlans>();
}


}
}
}
