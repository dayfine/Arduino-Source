/*  Door Approach Schedule
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Splits walking into a door or stairs into two separate durations:
 *
 *    - drive_hold: how long to hold the D-pad so the player reaches the warp
 *      from the farthest candidate starting tile, then releases automatically
 *      before the far map finishes loading (even if video capture stalls and
 *      the fade watcher cancels late).
 *    - fade_timeout: how long to keep watching for the warp fade (the drive
 *      hold plus the released wait).
 *    - dark_from: earliest time a dark screen counts as the door's fade,
 *      computed from the nearest candidate starting tile so a real early warp
 *      is never ignored.
 *
 */

#ifndef PokemonAutomation_Pokemon_DoorApproach_H
#define PokemonAutomation_Pokemon_DoorApproach_H

#include <cstddef>
#include <vector>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Movement and fade timing parameters for a game's door/stairs approach.
struct DoorApproachTiming{
    Milliseconds start_delay = Milliseconds(0);
    Milliseconds per_tile = Milliseconds(250);
    Milliseconds drive_margin = Milliseconds(500);
    Milliseconds fade_margin = Milliseconds(800);
    Milliseconds min_fade_wait = Milliseconds(4500);
    Milliseconds fallback_timeout = Milliseconds(12000);
    double dark_from_share = 0.85;
};

//  Computed schedule for one run into a door or stairs.
struct DoorApproachPlan{
    Milliseconds drive_hold = Milliseconds(12000);
    Milliseconds fade_timeout = Milliseconds(12000);
    Milliseconds dark_from = Milliseconds(0);

    bool operator==(const DoorApproachPlan& other) const = default;
};

//  Computes the door approach schedule from the candidate tile distances to
//  the door (ignoring 0 / unknown entries). When no positive distance is
//  available, falls back to holding for `timing.fallback_timeout` with
//  `dark_from = 0`.
DoorApproachPlan plan_door_approach(
    const std::vector<size_t>& tile_distances,
    const DoorApproachTiming& timing
);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
