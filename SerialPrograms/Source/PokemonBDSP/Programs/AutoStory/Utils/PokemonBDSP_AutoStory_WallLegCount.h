/*  BDSP AutoStory - Wall leg count
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a walk to a wall that stopped says about the tiles it moved.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_WallLegCount_H
#define PokemonAutomation_PokemonBDSP_AutoStory_WallLegCount_H

#include "Common/Cpp/Time.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Legs to a wall this many tiles or shorter must walk every tile: a tile
//  short is someone in the way, not the time count's slack. On lab-B
//  (2026-11-08, 01.08) Route 204's youngster stopped "west 2 (to wall)"
//  after 1 tile; a tile short passed as at the wall, the route logged
//  arriving at (11, 2) from (13, 2), and the taps north hit the cliff.
constexpr size_t EXACT_WALL_LEG_TILES = 3;

//  Tiles a walk to the wall moved, from the time it walked.
size_t tiles_moved(const WalkResult& walk, Milliseconds per_tile);

//  Tiles a walk on to the wall after a stop moved: none if no chunk moved.
size_t moved_on(const WalkResult& walk, Milliseconds per_tile);

//  Whether a walk to the wall never moved: no chunk changed and its time
//  counts no tile. It stopped where it began, so whoever stopped it stands
//  on the next tile, however short the leg.
bool never_moved(const WalkResult& walk, size_t walked_tiles);

//  Whether a walk to the wall that stopped took far longer than the tiles
//  to its nearest wall `nearest` allow, past the walker's slack.
bool held_up(const WalkResult& walk, size_t nearest, Milliseconds per_tile);

//  The fewest tiles a leg to a wall `expected_tiles` away must have walked
//  to count as at its wall.
size_t min_tiles_at_wall(size_t expected_tiles);

//  Whether a walk to the wall that ran out of time while still moving went
//  clearly past the farthest wall the map has, `farthest` tiles away: at a
//  wall it would have stopped. It is not where the plan has us (lab-B
//  2026-11-12 20:22, 01.02: the player had left the Gate unseen and kept
//  running through Oreburgh City).
bool ran_past_the_wall(const WalkResult& walk, size_t farthest, Milliseconds per_tile);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
