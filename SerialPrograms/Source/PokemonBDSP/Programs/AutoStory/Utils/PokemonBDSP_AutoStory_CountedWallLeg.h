/*  BDSP AutoStory - Counted wall leg
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A leg to a wall walked tile by tile, counting the tiles it moved.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_CountedWallLeg_H
#define PokemonAutomation_PokemonBDSP_AutoStory_CountedWallLeg_H

#include <cstddef>
#include <string>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_RouteTiming.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A leg held toward a wall the map has between `fewest` and `most` tiles
//  ahead (from each tile the player may stand on), its tiles counted by time
//  at `speed`.
struct CountedWallLeg{
    Pokemon::Direction direction;
    size_t fewest;
    size_t most;
    MoveSpeed speed;
    std::string label;
};
//  Walks `leg` to its wall and returns about how many tiles it took. Someone
//  walking about who stops it short is waited for, and the walk goes on
//  (Oreburgh's Hiker crosses column 46 on rows 25 to 27). Fails when the
//  count is far off the map's: a walk that went astray is not at its wall
//  (lab-B 2026-11-08 13:50: a hold east from (48, 57) moved 0 where the map
//  has (49, 57) open), nor is one through a door or into a dialogue.
size_t walk_counted_wall_leg(RouteIO& io, const CountedWallLeg& leg);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
