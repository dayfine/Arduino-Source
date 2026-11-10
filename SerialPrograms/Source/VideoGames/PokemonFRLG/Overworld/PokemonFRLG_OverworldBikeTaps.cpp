/*  Overworld Bicycle Taps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_BikeSpinPresses.h"
#include "PokemonFRLG_OverworldBikeTaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


StepOutcome count_bike_taps(const RouteStep& step, const BikeTapCalls& calls){
    int moved = 0;
    for (int tap = 1; tap <= step.tiles; tap++){
        BikeTap result = calls.tap(tap);
        for (int retry = 1; retry < calls.tries && !result.moved && !result.interrupt && !result.cleared_late; retry++){
            calls.log(std::format("Taps: tap {} of {} didn't move: tapping again, it may only have turned the player.", tap, step.tiles));
            result = calls.tap(tap);
        }
        if (result.interrupt || result.cleared_late){
            calls.log(std::format("Bike: tap {} of {}: interrupted after {} tile(s).", tap, step.tiles, moved));
            if (result.interrupt){
                calls.clear(*result.interrupt);
            }
            const int most = std::min(moved + 1, step.tiles);
            return StepOutcome{.end = StepEnd::STOPPED, .walked = WalkedTiles{moved, most}};
        }
        if (!result.moved){
            calls.log(std::format("Bike: tap {} of {} didn't move: blocked after {} tile(s).", tap, step.tiles, moved));
            return StepOutcome{.end = StepEnd::STOPPED, .walked = WalkedTiles{moved, moved + BLOCKED_TAP_SLACK}, .blocked = true};
        }
        moved++;
    }
    return StepOutcome{};
}

bool counts_step_taps(const RouteStep& step){
    return step.tapped && step.action == RouteAction::WALK && step.tiles > 0
        && !step.surfing && !step.warps && step.connection_steps.empty();
}

Milliseconds step_tap_settle(const RouteStep& step, int tap){
    using namespace MovementMechanics;
    if (tap != step.tiles || !step.spin_tiles){
        return Milliseconds(0);
    }
    return SPIN_TILE_TIME * *step.spin_tiles + SPIN_STOP_SETTLE;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
