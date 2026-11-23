/*  Door Approach Schedule
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "Pokemon_DoorApproach.h"

namespace PokemonAutomation{
namespace Pokemon{


DoorApproachPlan plan_door_approach(
    const std::vector<size_t>& tile_distances,
    const DoorApproachTiming& timing
){
    size_t min_tiles = 0;
    size_t max_tiles = 0;
    for (size_t tiles : tile_distances){
        if (tiles == 0){
            continue;
        }
        min_tiles = min_tiles == 0 ? tiles : std::min(min_tiles, tiles);
        max_tiles = std::max(max_tiles, tiles);
    }
    if (max_tiles == 0){
        return {
            .drive_hold = timing.fallback_timeout,
            .fade_timeout = timing.fallback_timeout,
            .dark_from = Milliseconds(0),
        };
    }

    const Milliseconds min_to_door = timing.start_delay + timing.per_tile * (int64_t)min_tiles;
    const Milliseconds max_to_door = timing.start_delay + timing.per_tile * (int64_t)max_tiles;
    const Milliseconds drive_hold = max_to_door + timing.drive_margin;
    const Milliseconds fade_timeout = std::max({
        timing.min_fade_wait,
        max_to_door + timing.fade_margin,
        drive_hold,
    });
    const double share = std::clamp(timing.dark_from_share, 0.0, 1.0);
    const Milliseconds dark_from = std::chrono::duration_cast<Milliseconds>(min_to_door * share);
    return {
        .drive_hold = drive_hold,
        .fade_timeout = fade_timeout,
        .dark_from = dark_from,
    };
}


} // namespace Pokemon
} // namespace PokemonAutomation
