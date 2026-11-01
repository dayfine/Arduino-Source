/*  Leg Walker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include "Pokemon_LegWalker.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string to_string(const LegSpec& leg){
    return std::format("{} {}{}", direction_name(leg.direction), leg.tiles, leg.to_wall ? " (to wall)" : "");
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

int steps_back_to_interrupt(const LegSpec& leg, Milliseconds elapsed, Milliseconds detection_delay){
    if (detection_delay < leg.step_window){
        return elapsed < leg.tile_time ? 1 : 0;
    }
    //  Steps land a tile after their press, a window apart.
    const Milliseconds before_landing = leg.tile_time - (elapsed - detection_delay);
    const int steps = (int)std::lround((double)before_landing.count() / leg.step_window.count());
    return std::max(steps, 0);
}


namespace{

//  An open leg walked a tile at a time. Since each interrupt is placed on the
//  step that set it off, the leg ends on its last tile even when an interrupt
//  is only seen after that step.
LegResult walk_leg_tile_by_tile(const LegSpec& leg, const LegCalls& calls, size_t max_interruptions){
    LegResult result;
    int walked = 0;
    //  Presses since the last interrupt: an interrupt can't reach back past
    //  one already cleared.
    int pressed = 0;
    while (walked < leg.tiles){
        if (leg.warps && walked == leg.tiles - 1){
            //  Onto the warp, whose fade would be taken for a battle's flash
            //  (console B, 2026-11-01: the player, already in Viridian
            //  Forest's north gate, tried to flee a battle).
            calls.hold(leg_hold(leg, 1));
            walked++;
            break;
        }
        const bool last_watched = walked == leg.tiles - (leg.warps ? 2 : 1);
        const std::optional<HoldOutcome> outcome = calls.hold_watching(
            leg_hold(leg, 1), leg.step_window + (last_watched ? leg.settle : Milliseconds::zero())
        );
        walked++;
        pressed++;
        if (!outcome){
            continue;
        }
        result.interruptions++;
        if (result.interruptions > max_interruptions){
            calls.fail(std::format("Interrupted {} times on: {}", result.interruptions, to_string(leg)));
        }
        calls.clear(outcome->interrupt);
        if (leg.stop_on_interrupt){
            calls.log(std::format("{} {}ms after a press, stopping the leg.", calls.name(outcome->interrupt), outcome->elapsed.count()));
            result.exact = false;
            result.stopped = true;
            return result;
        }
        const Milliseconds delay = calls.detection_delay(outcome->interrupt);
        walked -= std::min(steps_back_to_interrupt(leg, outcome->elapsed, delay), pressed);
        pressed = 0;
        if (delay >= leg.step_window){
            result.exact = false;
        }
        calls.log(std::format(
            "{} {}ms after a press, {} of {} tiles walked.",
            calls.name(outcome->interrupt), outcome->elapsed.count(), walked, leg.tiles
        ));
    }
    result.tiles = leg.tiles;
    return result;
}

}


LegResult walk_leg_with(const LegSpec& leg, const LegCalls& calls, size_t max_interruptions){
    if (!leg.to_wall && leg.step_window > Milliseconds::zero()){
        return walk_leg_tile_by_tile(leg, calls, max_interruptions);
    }
    LegResult result;
    int remaining = leg.tiles;
    while (true){
        const Milliseconds hold = leg_hold(leg, remaining);
        const std::optional<HoldOutcome> outcome = calls.hold_watching(hold, hold);
        if (!outcome){
            result.tiles = leg.tiles;
            return result;
        }
        result.interruptions++;
        if (result.interruptions > max_interruptions){
            calls.fail(std::format("Interrupted {} times on: {}", result.interruptions, to_string(leg)));
        }
        calls.clear(outcome->interrupt);
        const std::string seen = std::format("{} after {}ms", calls.name(outcome->interrupt), outcome->elapsed.count());
        if (!leg.to_wall && leg.stop_on_interrupt){
            calls.log(seen + ", stopping the leg.");
            result.exact = false;
            result.stopped = true;
            return result;
        }
        if (leg.to_wall && leg.stop_if_blocked && result.interruptions > 1 && outcome->elapsed < leg.tile_time * 3){
            calls.log(seen + ", again at once: something on the way stops every hold.");
            result.exact = false;
            result.stopped = true;
            return result;
        }
        if (leg.to_wall){
            //  The wall stops the player on the leg's last tile however far
            //  they got, so the leg is simply held again.
            calls.log(seen + ", walking to the wall again.");
            continue;
        }
        const int walked = tiles_before_interrupt(leg, outcome->elapsed, calls.detection_delay(outcome->interrupt), remaining);
        remaining -= walked;
        result.exact = false;
        calls.log(std::format("{}, about {} tiles walked, {} to go.", seen, walked, remaining));
        if (remaining <= 0){
            result.tiles = leg.tiles;
            return result;
        }
    }
}



}
}
