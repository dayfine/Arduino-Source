/*  Route Log Legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Split out of Pokemon_RouteLog.h, unchanged; it includes this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_RouteLogLegs_H
#define PokemonAutomation_Pokemon_RouteLogLegs_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_RouteLogTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


// Description of a single route movement segment.
struct RouteLogLeg{
    RouteLegAction action = RouteLegAction::WALK;
    Direction direction = Direction::NORTH;
    size_t tiles = 0;
    RouteLegEnd end = RouteLegEnd::OPEN;
};
//  "east 15 (to wall)", "south 4 (open)", "north 1 (warp)", "surf east",
//  "face west".
std::string to_string(const RouteLogLeg& leg);

//  A planned leg and the tiles it may end on.
struct PlannedRouteLeg{
    RouteLogLeg leg;
    std::vector<RouteTile> after;
};

//  How the end of a walked leg is known.
enum class EndBasis{
    WALKED,     //  walked as planned, or seen
    ASSUMED,    //  taken to have ended as planned after an interrupt
    UNKNOWN,    //  stopped somewhere on the way
};
// Returns the readable string representation of an end basis.
std::string basis_name(EndBasis basis);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
