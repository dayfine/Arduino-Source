/*  BDSP AutoStory Indoors Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon/Navigation/Pokemon_MapSet.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "StorySegments/PokemonBDSP_AutoStory_SandgemCenterEvents.h"
#include "Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Indoors_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::MapSet;
using Pokemon::TilePos;

//  00.04c's Potions: the Mart lady's walk (after_catch_potions() and the Mart lady checkpoint)
//  is walks until stopped, so on the grid each leg ends at its wall: from
//  a tile to three below the Center's door, a tap east and north to the
//  Center's front, west to the Lab's sign, a row south and west to
//  her grass's edge, north to the trees on her row (she stands east of
//  there), back a row south and east to Route 201's sign, a row south and
//  east to the trees east of the Mart, and north to the front row's end.
struct LadyLeg{
    Direction direction;
    size_t tiles;           //  0: walk until the wall
    TilePos end;            //  world tile
    std::string name;
};

class Test_MartLadyWalk : public UnitTest{
public:
    Test_MartLadyWalk()
        : UnitTest("PokemonBDSP::AutoStory - 00.04: the walk to the Poke Mart lady and back")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const MapSet& maps = story_maps();
        const auto sandgem = [&](const TilePos& pos){ return maps.to_world(MAP_SANDGEM_TOWN, pos); };
        const auto route201 = [&](const TilePos& pos){ return maps.to_world(MAP_ROUTE_201, pos); };
        const std::vector<LadyLeg> legs{
            {Direction::EAST, 1, sandgem({18, 0}), "a tap east under the Center's wall"},
            {Direction::NORTH, 0, sandgem({18, 11}), "north to the Center's front"},
            {Direction::WEST, 0, east_of_lab_sign(), "west to the Lab's sign"},
            {Direction::SOUTH, 1, sandgem({6, 12}), "a row south onto the path"},
            {Direction::WEST, 0, route201({42, 5}), "west to her grass's edge"},
            {Direction::NORTH, 0, route201({42, 3}), "north to the trees, on her row"},
            {Direction::SOUTH, 1, route201({42, 4}), "a row south onto the signs' row"},
            {Direction::EAST, 0, route201_sign_west(), "east to Route 201's sign"},
            {Direction::SOUTH, 1, route201({54, 5}), "a row south onto the path"},
            {Direction::EAST, 0, sandgem({29, 12}), "east to the trees east of the Mart"},
            {Direction::NORTH, 0, sandgem(SandgemTown::FRONT_ROW_EAST_END), "north to the front row's end"},
        };
        //  Out of the Center's door we stand a tile to three below it (r261).
        for (int below = 1; below <= 3; below++){
            const std::string error = follow(maps, legs, sandgem({17, 10 + below}), logger);
            if (!error.empty()){
                return "From " + std::to_string(below) + " below the door: " + error;
            }
        }
        return true;
    }

private:
    static std::string follow(const MapSet& maps, const std::vector<LadyLeg>& legs, TilePos at, Logger& logger){
        for (const LadyLeg& leg : legs){
            if (leg.tiles == 0){
                at = Pokemon::walk_to_wall(maps.world(), at, leg.direction).stop;
            }else{
                const std::optional<TilePos> next = maps.world().step(at, leg.direction);
                if (!next){
                    return leg.name + ": blocked at " + maps.describe(at) + ".";
                }
                at = *next;
            }
            //  The tap east lands on whichever row we stood on.
            if (&leg != &legs.front() && !(at == leg.end)){
                return leg.name + ": ends at " + maps.describe(at) + ", not " + maps.describe(leg.end) + ".";
            }
            logger.log(leg.name + ": " + maps.describe(at));
        }
        return "";
    }
};

}


void add_tests_AutoStory_Indoors(UnitTestDatabase& database){
    database.add<Test_MartLadyWalk>();
}


}
}
}
