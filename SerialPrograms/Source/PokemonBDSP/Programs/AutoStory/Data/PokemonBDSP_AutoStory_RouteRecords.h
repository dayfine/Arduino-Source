/*  BDSP AutoStory - Route records
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Every walk a checkpoint makes that is not planned on a grid, written
 *  down as legs: the direction, the count or the thing that ends the leg
 *  (a wall, a person, a door, a location banner, a map transition, a
 *  scene). The checkpoints run their own code today; these records say
 *  what that code does, so a test can trace each leg on the map once the
 *  map is right, and so the next version can run the record instead.
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteRecords_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteRecords_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
namespace RouteRecords{


//  What ends a leg.
enum class LegEnd{
    COUNT,          //  exactly `tiles` tiles (D-pad taps, or a stick hold timed for them)
    WALL,           //  walks until the map stops the player; `tiles` is the expected count, 0 if unknown
    NPC,            //  walks until a person stops the player
    DOOR,           //  walks into a door or cave mouth (a fade)
    BANNER,         //  walks until the next map's location banner shows
    TRANSITION,     //  walks until a map transition (the screen goes dark)
    SCENE,          //  walks until a cutscene takes over
};

struct RouteLeg{
    Pokemon::Direction direction;
    size_t tiles;
    LegEnd end;
    std::string note;
    //  A counted leg walked by D-pad taps (or a leg the map ends) is exact;
    //  a count from a timed stick hold is an estimate, a tile or two either
    //  way, until someone counts it on the console in taps.
    bool exact = true;
};

struct RouteRecord{
    std::string checkpoint;         //  "01.05"
    std::string title;
    std::string map;                //  the map the legs are on ("" when none exists yet)
    const Pokemon::TileGrid* (*grid)();     //  that map's grid, nullptr when none
    bool traceable;                 //  the grid's rows are the game's (decomp or console), so legs can be traced
    std::optional<Pokemon::TilePos> start;  //  where the record starts on that grid
    std::vector<RouteLeg> legs;
    std::string source;             //  the code and the console runs behind it
};

const std::vector<RouteRecord>& all();


}
}
}
}
#endif
