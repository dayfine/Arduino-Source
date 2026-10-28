/*  BDSP AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
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

using Pokemon::GridDirection;
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
            {MAP_ROUTE_201,     Route201::POTION_GIVER_TALK_SPOT,   TileType::TALL_GRASS},
            {MAP_SANDGEM_TOWN,  SandgemTown::LAB_DOOR,              TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKEMON_CENTER_DOOR,   TileType::DOOR},
            {MAP_SANDGEM_TOWN,  SandgemTown::POKE_MART_DOOR,        TileType::DOOR},
            {MAP_ROUTE_202,     Route202::CATCH_SPOT,               TileType::TALL_GRASS},
            {MAP_ROUTE_202,     Route202::TRAINER_A,                TileType::NPC},
            {MAP_ROUTE_202,     Route202::TRAINER_B,                TileType::NPC},
            {MAP_ROUTE_202,     Route202::TRAINER_C,                TileType::NPC},
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
        if (Route201::POTION_GIVER_TALK_SPOT.next(GridDirection::WEST) != Route201::POTION_GIVER){
            return "The Potion giver's talk spot is not east of her.";
        }

        //  The maps join where their exits meet.
        struct Join{
            const char* from_map;
            TilePos from;
            GridDirection direction;
            const char* to_map;
            TilePos to;
        };
        const Join joins[] = {
            {MAP_ROUTE_201,     {63, 4},    GridDirection::EAST,    MAP_SANDGEM_TOWN,   {0, 7}},
            {MAP_ROUTE_201,     {63, 6},    GridDirection::EAST,    MAP_SANDGEM_TOWN,   {0, 9}},
            {MAP_SANDGEM_TOWN,  {24, 0},    GridDirection::NORTH,   MAP_ROUTE_202,      {25, 25}},
            {MAP_SANDGEM_TOWN,  {29, 0},    GridDirection::NORTH,   MAP_ROUTE_202,      {30, 25}},
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
            GridDirection direction;
            TilePos stop;
            size_t tiles;
            bool anchored;
        };
        const TileGrid& small = small_grid();
        const TileGrid& route = Route202::grid();
        const Case cases[] = {
            {small, {1, 1}, GridDirection::EAST,  {5, 1}, 4, true},     //  through grass to the east wall
            {small, {1, 0}, GridDirection::SOUTH, {1, 3}, 3, true},     //  hops the ledge, stops at the sign
            {small, {2, 3}, GridDirection::NORTH, {2, 3}, 0, true},     //  a ledge stops a north walk
            {small, {3, 3}, GridDirection::SOUTH, {3, 3}, 0, true},     //  the NPC is a wall
            {small, {4, 4}, GridDirection::NORTH, {4, 0}, 4, false},    //  through the gap, off the top edge
            {small, {4, 3}, GridDirection::EAST,  {5, 3}, 1, true},
            {small, {4, 4}, GridDirection::EAST,  {4, 4}, 0, false},    //  into the door
            {small, {2, 0}, GridDirection::EAST,  {2, 0}, 0, false},    //  off the map
            {route, Route202::CATCH_SPOT, GridDirection::WEST,  {3, 21}, 15, true},
            {route, Route202::CATCH_SPOT, GridDirection::NORTH, {18, 20}, 1, true},
            {route, {7, 12}, GridDirection::NORTH, {7, 12}, 0, true},  //  trainer_a holds the player
            //  Through the gap east of the north ledge, over the south ledge
            //  and off the bottom edge to Sandgem Town.
            {route, {29, 4}, GridDirection::SOUTH, {29, 25}, 21, false},
            {route, {12, 8}, GridDirection::SOUTH, {12, 22}, 14, true},  //  hops both ledges
            {route, Route202::EXIT_JUBILIFE, GridDirection::NORTH, Route202::EXIT_JUBILIFE, 0, false},
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
        if (dx != 0 || dy != 2 || grid.at(a.next(GridDirection::SOUTH)) != TileType::LEDGE){
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
        if (south->size() != 1 || south->front().direction != GridDirection::SOUTH){
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
        const TilePos potion = maps.to_world(MAP_ROUTE_201, Route201::POTION_GIVER_TALK_SPOT);
        const TilePos jubilife = maps.to_world(MAP_ROUTE_202, Route202::EXIT_JUBILIFE);
        error = check_plan(logger, "Catch spot to Potion giver", maps.world(), catch_spot, potion, &maps, trainers);
        if (!error.empty()){
            return error;
        }
        error = check_plan(logger, "Potion giver to Jubilife", maps.world(), potion, jubilife, &maps, trainers);
        if (!error.empty()){
            return error;
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
