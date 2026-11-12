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


namespace{

//  The facing before a tap, if `calls` reads it.
std::optional<Direction> facing_before(const BikeTapCalls& calls){
    if (!calls.facing){
        return std::nullopt;
    }
    return calls.facing();
}

} // namespace

StepOutcome count_bike_taps(const RouteStep& step, const BikeTapCalls& calls){
    int moved = 0;
    for (int tap = 1; tap <= step.tiles; tap++){
        std::optional<Direction> before = facing_before(calls);
        BikeTap result = calls.tap(tap);
        for (int retry = 1; retry < calls.tries && !result.moved && !result.interrupt && !result.cleared_late; retry++){
            if (!before || *before == step.direction){
                calls.log(std::format(
                    "Taps: tap {} of {} showed no step, facing {} before it: not tapping again, it may have stepped unseen.",
                    tap, step.tiles, before ? Pokemon::direction_name(*before) : "an unknown way"
                ));
                break;
            }
            calls.log(std::format("Taps: tap {} of {} didn't move: tapping again, it only turned the player.", tap, step.tiles));
            before = facing_before(calls);
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
        if (!result.moved && calls.facing && !step.to_wall){
            //  On foot, an open leg's tap that showed no step may have
            //  stepped unseen: not a wall ahead, which would keep only the
            //  tiles with one (09.02 run 8, console B, 2026-11-11 18:02:
            //  "west 2" on Rocket Hideout B2F taken as at (17, 5), a tile
            //  east of the player, so the arrow was missed).
            calls.log(std::format("Taps: tap {} of {} showed no step: stopped after {} or {} tile(s).", tap, step.tiles, moved, moved + 1));
            return StepOutcome{.end = StepEnd::STOPPED, .walked = WalkedTiles{moved, moved + 1}};
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
