/*  BDSP AutoStory Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_Route202.h"
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
//  Row 2 is a ledge with a gap at x = 4; the NPC at (3,4) and the sign at
//  (1,4) stand on the bottom row.
const TileGrid& small_grid(){
    static const TileGrid grid({
        R"(#.....#)",
        R"(#.."".#)",
        R"(#vvv..#)",
        R"(#.....#)",
        R"(#S.N..#)",
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
            {{-1, 0}, TileType::BLOCKED},
            {{7, 4}, TileType::BLOCKED},
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

        const TileGrid& route = Route202::grid();
        if (route.width() != 36 || route.height() != 26){
            return "Route 202 is " + std::to_string(route.width()) + "x" + std::to_string(route.height()) + ".";
        }
        const Case named[] = {
            {Route202::CATCH_SPOT,          TileType::TALL_GRASS},
            {Route202::POTION_GIVER,        TileType::NPC},
            {Route202::POTION_GIVER_FRONT,  TileType::WALKABLE},
            {Route202::TRAINER_TRISTAN,     TileType::NPC},
            {Route202::TRAINER_NATALIE,     TileType::NPC},
            {Route202::TRAINER_LOGAN,       TileType::NPC},
            {Route202::EXIT_JUBILIFE,       TileType::WALKABLE},
        };
        for (const Case& c : named){
            if (route.at(c.pos) != c.type){
                return "Route 202 tile " + Pokemon::to_string(c.pos) + " is type " + std::to_string((int)route.at(c.pos)) + ".";
            }
        }
        if (Route202::POTION_GIVER_FRONT.next(GridDirection::NORTH) != Route202::POTION_GIVER){
            return "The Potion giver's front tile is not south of her.";
        }
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
            bool wall_in_map;
        };
        const TileGrid& small = small_grid();
        const TileGrid& route = Route202::grid();
        const Case cases[] = {
            {small, {1, 1}, GridDirection::EAST,  {5, 1}, 4, true},     //  through grass to the east wall
            {small, {1, 0}, GridDirection::SOUTH, {1, 3}, 3, true},     //  hops the ledge, stops at the sign
            {small, {2, 3}, GridDirection::NORTH, {2, 3}, 0, true},     //  a ledge stops a north walk
            {small, {3, 3}, GridDirection::SOUTH, {3, 3}, 0, true},     //  the NPC is a wall
            {small, {4, 4}, GridDirection::NORTH, {4, 0}, 4, false},    //  through the gap, off the top edge
            {route, Route202::CATCH_SPOT, GridDirection::WEST,  {3, 21}, 15, true},
            {route, Route202::CATCH_SPOT, GridDirection::NORTH, {18, 20}, 1, true},
            {route, Route202::POTION_GIVER_FRONT, GridDirection::NORTH, Route202::POTION_GIVER_FRONT, 0, true},
            //  Through the gap east of the north ledge, over the south ledge
            //  and off the bottom edge to Sandgem Town.
            {route, {29, 4}, GridDirection::SOUTH, {29, 25}, 21, false},
            {route, {12, 8}, GridDirection::SOUTH, {12, 22}, 14, true},  //  hops both ledges
            {route, Route202::EXIT_JUBILIFE, GridDirection::NORTH, Route202::EXIT_JUBILIFE, 0, false},
        };
        for (const Case& c : cases){
            const Pokemon::WallHit hit = Pokemon::walk_to_wall(c.grid, c.from, c.direction);
            if (hit.stop != c.stop || hit.tiles != c.tiles || hit.wall_in_map != c.wall_in_map){
                return "Walking " + std::string(Pokemon::direction_name(c.direction)) + " from " +
                    Pokemon::to_string(c.from) + " stops at " + Pokemon::to_string(hit.stop) + " after " +
                    std::to_string(hit.tiles) + " tiles" + (hit.wall_in_map ? "" : " (map edge)") +
                    ", expected " + Pokemon::to_string(c.stop) + " after " + std::to_string(c.tiles) + ".";
            }
        }
        return true;
    }
};

//  Checks a plan from `from` to `to`: every leg can be walked, ledges are
//  crossed only southward, and each leg's wall flag matches the grid.
std::string check_plan(
    Logger& logger, const std::string& name,
    const TileGrid& grid, const TilePos& from, const TilePos& to
){
    const std::optional<std::vector<TileLeg>> legs = Pokemon::plan_tile_route(grid, from, to);
    if (!legs){
        return name + ": no route found.";
    }
    std::string text = name + " " + Pokemon::to_string(from) + " -> " + Pokemon::to_string(to) + ":";
    for (const TileLeg& leg : *legs){
        text += " " + Pokemon::to_string(leg) + ";";
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

        const TileGrid& route = Route202::grid();
        error = check_plan(logger, "Catch spot to Potion giver", route, Route202::CATCH_SPOT, Route202::POTION_GIVER_FRONT);
        if (!error.empty()){
            return error;
        }
        error = check_plan(logger, "Potion giver to Jubilife", route, Route202::POTION_GIVER_FRONT, Route202::EXIT_JUBILIFE);
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
