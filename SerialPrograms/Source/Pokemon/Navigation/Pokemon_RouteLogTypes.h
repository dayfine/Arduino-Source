/*  Route Log Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The types of a route log's lines. Pokemon_RouteLog.h builds on it.
 *
 */

#ifndef PokemonAutomation_Pokemon_RouteLogTypes_H
#define PokemonAutomation_Pokemon_RouteLogTypes_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


// Prefix tag prepended to route navigation log messages.
inline constexpr std::string_view ROUTE_LOG_TAG = "[Route] ";

//  A tile of a named map.
struct RouteTile{
    std::string map;
    TilePos pos;
};
//  "Route 202 (23, 25)".
std::string to_string(const RouteTile& tile);
//  The tiles, comma-separated; "an unknown tile" for none.
std::string describe(const std::vector<RouteTile>& tiles);


// Action type performed during a route leg.
enum class RouteLegAction{
    WALK,
    SURF,
    FACE,
    //  Face a gate and press A: a card-key gate opens.
    OPEN_GATE,
    //  Face a statue and press A, then Yes: a switch flips.
    PRESS_SWITCH,
};

//  How a leg ends: at a wall, on a warp, or where it is timed to.
enum class RouteLegEnd{
    OPEN,
    WALL,
    WARP,
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
