/*  Overworld Bicycle Taps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "PokemonFRLG_OverworldBikeTaps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


StepOutcome count_bike_taps(const RouteStep& step, const BikeTapCalls& calls){
    int moved = 0;
    for (int tap = 1; tap <= step.tiles; tap++){
        const BikeTap result = calls.tap();
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
