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

//  indoors() decides when leave_door_south() is out: r257 walked on south
//  past Sandgem's road into a house after leaving the Center, because the
//  evening tint made the outside as warm as a room. Synthetic screens: the
//  evening outside (warm all over), a room (warm, black along the bottom),
//  a fade (black all over), and warm with a thin dark edge (a shadow).
struct IndoorsCase{
    std::string name;
    uint32_t fill;
    double dark_bottom;     //  how much of the screen's height, from the bottom, is black
    bool indoors;
};

class Test_IndoorsAnyLight : public UnitTest{
public:
    Test_IndoorsAnyLight()
        : UnitTest("PokemonBDSP::AutoStory - indoors() under the evening tint")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<IndoorsCase> cases{
            {.name = "evening outside, orange all over", .fill = 0xffe0a040, .dark_bottom = 0, .indoors = false},
            {.name = "a room: orange, black below its floor", .fill = 0xffe0a040, .dark_bottom = 0.10, .indoors = true},
            {.name = "a fade: black all over", .fill = 0xff000000, .dark_bottom = 0, .indoors = false},
            {.name = "orange with a thin shadow at the bottom", .fill = 0xffe0a040, .dark_bottom = 0.01, .indoors = false},
            {.name = "daytime grass", .fill = 0xff60b040, .dark_bottom = 0.10, .indoors = false},
        };
        for (const IndoorsCase& item : cases){
            ImageRGB32 screen(1920, 1080);
            const size_t dark_from = (size_t)(1080 * (1 - item.dark_bottom));
            for (size_t y = 0; y < 1080; y++){
                for (size_t x = 0; x < 1920; x++){
                    screen.pixel(x, y) = y >= dark_from ? 0xff000000 : item.fill;
                }
            }
            if (indoors(screen) != item.indoors){
                return item.name + (item.indoors ? ": read as outside." : ": read as inside.");
            }
        }
        return true;
    }
};

//  door_fade_look(): out of a door is a dark look, then a bright one that is
//  not a room. r259 turned round in Sandgem's lit Center, read "not a room"
//  once at full brightness, and walked east inside it as if outside.
struct DoorLook{
    double brightness;
    bool room;
};
struct DoorCase{
    std::string name;
    std::vector<DoorLook> looks;
    //  The look (1-based) that reads out, or 0 for none.
    size_t out_at;
};

class Test_DoorFade : public UnitTest{
public:
    Test_DoorFade()
        : UnitTest("PokemonBDSP::AutoStory - out of a door needs its fade")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<DoorCase> cases{
            {.name = "r259: a bright look in the Center that misses the room", .looks = {{458, true}, {440, false}, {457, true}}, .out_at = 0},
            {.name = "the fade, then the town", .looks = {{458, true}, {300, true}, {60, false}, {310, false}}, .out_at = 4},
            {.name = "the fade, then the room again", .looks = {{458, true}, {60, false}, {420, true}}, .out_at = 0},
            {.name = "the fade, still dim", .looks = {{60, false}, {150, false}, {320, false}}, .out_at = 3},
        };
        for (const DoorCase& item : cases){
            DoorFadeWatch watch;
            size_t out_at = 0;
            for (size_t i = 0; i < item.looks.size() && out_at == 0; i++){
                if (door_fade_look(watch, item.looks[i].brightness, item.looks[i].room)){
                    out_at = i + 1;
                }
            }
            if (out_at != item.out_at){
                return item.name + ": out at look " + std::to_string(out_at) + ", expected " + std::to_string(item.out_at) + ".";
            }
        }
        return true;
    }
};

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
    database.add<Test_IndoorsAnyLight>();
    database.add<Test_DoorFade>();
    database.add<Test_MartLadyWalk>();
}


}
}
}
