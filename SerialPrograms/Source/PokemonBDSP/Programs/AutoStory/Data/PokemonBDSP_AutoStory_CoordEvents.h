/*  BDSP AutoStory - Coord events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scene triggers of the story maps: stepping onto one of an event's
 *  tiles while its story variable holds `value` starts its script. They are
 *  the decomp's coord events (zone_event), read from a map's
 *  "coord_events", which the shared map schema leaves to the game:
 *
 *      "coord_events": [{"name": "catch_tutorial", "x": 21, "y": 25, "width": 1, "length": 5,
 *                        "var": "VAR_ROUTE_202_STATE", "value": 0, "source": "..."}, ...]
 *
 *  A trigger seen on the console that the decomp does not have names the run
 *  in `source`.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_CoordEvents_H
#define PokemonAutomation_PokemonBDSP_AutoStory_CoordEvents_H

#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Coordinate-triggered event parsed from BDSP map data.
struct CoordEvent{
    std::string name;
    //  The map's own tiles: `width` columns from x, `length` rows from y.
    std::vector<Pokemon::TilePos> tiles;
    std::string var;
    int value = 0;
    std::string source;
};

//  The coord events of the map `map` (a JSON map's name), none if it lists
//  none. Throws InternalProgramError for an unknown map, and ParseException
//  for an event that does not parse.
const std::vector<CoordEvent>& story_coord_events(std::string_view map);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
