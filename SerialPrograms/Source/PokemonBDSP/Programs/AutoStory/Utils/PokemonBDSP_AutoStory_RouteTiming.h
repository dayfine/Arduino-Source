/*  BDSP AutoStory - Route timing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walking and running times per tile, and the stick for a direction.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteTiming_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteTiming_H

#include <cstddef>
#include "Common/Cpp/Time.h"
#include "Controllers/Joystick.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MoveMechanics.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Walking time per tile, measured on the console from the post-catch save:
//  a row is deeper than a column is wide under the tilted camera.
constexpr Milliseconds ROUTE_MS_PER_TILE_ACROSS = Pokemon::BDSP_MOVE_MECHANICS.walk.tile.across;
//  Walking time per vertical tile (north or south).
constexpr Milliseconds ROUTE_MS_PER_TILE_UP_DOWN = Pokemon::BDSP_MOVE_MECHANICS.walk.tile.up_down;
//  From pressing the stick to the first step, when standing still.
constexpr Milliseconds ROUTE_START_DELAY = Pokemon::BDSP_MOVE_MECHANICS.start_delay;
//  How fast the player moves on a leg to a wall (B held with the D-pad).
//  B only runs once Mom has given the Running Shoes (00.03); before that
//  it walks. The bike comes later, with its own measured times.
enum class MoveSpeed{
    WALK,
    RUN,
};

//  Running time per tile, measured on the console: r27 ran Twinleaf's
//  column 17 north onto Route 201, 35 tiles in about 5.75 s (164 ms a
//  tile) where walking took 240. Across is scaled the same until measured.
//  Rounded down: a count from too fast a pace runs over, which a wall leg
//  forgives, where one from too slow a pace stops short of the wall.
namespace detail{
//  Running time per tile across.
constexpr Milliseconds ROUTE_RUN_MS_PER_TILE_ACROSS = Milliseconds(100);
//  Running time per vertical tile.
constexpr Milliseconds ROUTE_RUN_MS_PER_TILE_UP_DOWN = Milliseconds(150);
} // namespace detail

//  Time per tile in `direction` at `speed`.
Milliseconds route_ms_per_tile(Pokemon::Direction direction, MoveSpeed speed = MoveSpeed::WALK);
//  The longest `tiles` tiles at `speed` take to hold, from standing: a
//  limit for a run that something on the way ends (a scene's dialogue).
Milliseconds route_limit_for_tiles(MoveSpeed speed, size_t tiles);
//  The left stick for a direction. Row 0 is the top of a map, and the
//  stick's y is positive upward.
JoystickPosition route_stick(Pokemon::Direction direction);

//  How long to hold the stick for an open leg of `tiles` tiles, for a player
//  already facing that way: a step once started is always finished, so stop
//  half a tile early. On the console a 1290ms hold west went 9 tiles and an
//  800ms hold south 4: the player starts at once and finishes the step.
Milliseconds route_open_hold(Pokemon::Direction direction, size_t tiles);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
