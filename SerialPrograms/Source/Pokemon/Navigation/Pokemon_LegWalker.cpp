/*  Leg Walker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_LegWalker.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string to_string(const LegSpec& leg){
    return std::string(direction_name(leg.direction)) + " " + std::to_string(leg.tiles)
        + (leg.to_wall ? " (to wall)" : "");
}


Milliseconds leg_hold(const LegSpec& leg, int tiles){
    if (leg.to_wall){
        return leg.tile_time * (tiles + 2);
    }
    return leg.tile_time * (tiles - 1) + leg.tile_time / 2;
}

int tiles_before_interrupt(const LegSpec& leg, Milliseconds elapsed, Milliseconds detection_delay, int remaining){
    const Milliseconds since_first_step = elapsed - detection_delay + leg.tile_time / 2;
    if (since_first_step.count() <= 0){
        return 0;
    }
    return std::clamp((int)(since_first_step / leg.tile_time), 0, remaining);
}


LegResult walk_leg_with(const LegSpec& leg, const LegCalls& calls, size_t max_interruptions){
    LegResult result;
    int remaining = leg.tiles;
    while (true){
        const std::optional<HoldOutcome> outcome = calls.hold_watching(leg_hold(leg, remaining));
        if (!outcome){
            result.tiles = leg.tiles;
            return result;
        }
        result.interruptions++;
        if (result.interruptions > max_interruptions){
            calls.fail("Interrupted " + std::to_string(result.interruptions) + " times on: " + to_string(leg));
        }
        calls.clear(outcome->interrupt);
        const std::string seen = calls.name(outcome->interrupt) + " after " + std::to_string(outcome->elapsed.count()) + "ms";
        if (leg.to_wall){
            //  The wall stops the player on the leg's last tile however far
            //  they got, so the leg is simply held again.
            calls.log(seen + ", walking to the wall again.");
            continue;
        }
        const int walked = tiles_before_interrupt(leg, outcome->elapsed, calls.detection_delay(outcome->interrupt), remaining);
        remaining -= walked;
        result.exact = false;
        calls.log(seen + ", about " + std::to_string(walked) + " tiles walked, " + std::to_string(remaining) + " to go.");
        if (remaining <= 0){
            result.tiles = leg.tiles;
            return result;
        }
    }
}



}
}
