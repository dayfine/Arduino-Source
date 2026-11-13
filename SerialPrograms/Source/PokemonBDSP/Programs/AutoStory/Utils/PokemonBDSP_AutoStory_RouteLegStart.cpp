/*  BDSP AutoStory - Route leg start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <optional>
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonBDSP_AutoStory_RouteLegStart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


void leg_started(RouteIO& io, size_t number, size_t count, const PlannedRouteLeg& leg, StoryPausePoints* pause){
    io.note(route_leg_line(number, count, leg), RouteNote::STATUS);
    if (pause == nullptr){
        return;
    }
    std::optional<TilePos> end;
    if (leg.after.size() == 1){
        end = leg.after[0].pos;
    }
    pause->before_leg(story_leg_pause_label(number - 1, count, leg.leg.direction, (int)leg.leg.tiles, end));
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
