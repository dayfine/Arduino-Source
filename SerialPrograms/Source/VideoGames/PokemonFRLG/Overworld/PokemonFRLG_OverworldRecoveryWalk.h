/*  Overworld Recovery Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console side of run_route() (PokemonFRLG_OverworldRecovery.h): walks
 *  a route with the walker's steps (PokemonFRLG_OverworldWalk.h), except
 *  that an open leg stops at its first interruption, and the route finds
 *  the player again around where the timing puts them, instead of walking
 *  on from a count.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldRecoveryWalk_H
#define PokemonAutomation_PokemonFRLG_OverworldRecoveryWalk_H

#include <optional>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldRecovery.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  TODO(forest drift, 2026-11-01): the threshold is a diagnostic's guess.
//  How much each edge quarter of the game screen, clear of the player in the
//  middle, may change when a tap toward a wall only bumps the player (the
//  player's own animation, an animated tile). A step scrolls the whole map
//  by a tile. Not yet measured on the console: a guess on the safe side.
constexpr double WALL_TAP_MOVED_RMSD = 20.0;

//  How much a counted tap scrolled the map: the largest RMSD between `before`
//  and `after` over the four edge quarters (top, bottom, left, right) of
//  `game`, all clear of the player in the middle. One band alone misses a
//  step over a floor that repeats every tile (Rocket Hideout B3F, 09.02): a
//  one-tile scroll leaves the band nearly the same.
double tap_scroll_change(const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game);


//  Walks `route`, planned with `planning`, at `speed`, as
//  walk_overworld_route() does, from the same start: standing still on the
//  overworld, not on the bike, facing `facing` (read from the screen if not
//  given).
//
//  When a battle or a message stops an open leg part-way, where the player
//  is isn't guessed: the stop's timing narrows it to a few tiles of the leg
//  (walked_near()), from which run_route() finds them again with
//  `planning`'s maps and costs, and walks the rest of the route as planned.
//  A warp's fade that turns out to be a wild battle's stops its leg the
//  same way, somewhere before the warp (walked_before_warp()).
//
//  After a leg that crosses a trainer's sight it watches for the trainer,
//  long enough for them to walk up and speak, and hands what shows to
//  `interrupts`; where the maps place the stop, the route goes on from
//  there (run_route()).
//
//  After each leg to a wall it taps once more toward the wall, and logs
//  whether the screen moved (a diagnostic; RoutePlanning::wall_check SKIP
//  turns it off, as the Fast checks level does).
//
//  A message open before a step is handed to `interrupts` once
//  (clear_open_message()). Throws OperationFailedException if a menu or
//  message is still open before a step, a warp's fade or the Surf prompt
//  doesn't show, a leg is interrupted too many times, or the player can't
//  be found again from every tile they may be on. A wild battle that
//  starts as the last leg ends is cleared before it returns.
void walk_overworld_route_with_recovery(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    const RoutePlanning& planning,
    std::optional<Direction> facing = std::nullopt,
    const OverworldInterruptSet& interrupts = default_overworld_interrupts(),
    MoveSpeed speed = MoveSpeed::WALK
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
