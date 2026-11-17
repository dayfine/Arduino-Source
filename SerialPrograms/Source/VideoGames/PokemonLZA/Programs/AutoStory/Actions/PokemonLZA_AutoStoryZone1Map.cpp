/*  PokemonLZA AutoStory - Wild Zone 1 map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryZone1Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::optional<MapWaypoint> catch_spot_waypoint(int spot){
    switch (spot){
    case 100:
        return ZONE1_SPOT_100;
    case 60:
        return ZONE1_SPOT_60;
    default:
        return std::nullopt;
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
