/*  AutoStory Fly Landing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonFRLG_AutoStoryFlyLanding.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


bool fly_faded(const Pokemon::ScreenLook& look){
    for (const Pokemon::ScreenEventChange& change : look.log.changes()){
        const bool fade = change.event == Pokemon::ScreenEvent::FADE_BLACK || change.event == Pokemon::ScreenEvent::FADE_WHITE;
        if (fade && change.edge == Pokemon::ScreenEventEdge::ROSE){
            return true;
        }
    }
    return false;
}

bool FlyLanding::landed() const{
    return faded && facing.has_value();
}

FlyLanding judge_fly_landing(
    const Pokemon::ScreenLook& look, Milliseconds quiet,
    const std::function<std::optional<Pokemon::Direction>()>& read_facing
){
    FlyLanding out;
    out.faded = fly_faded(look);
    out.settle = Pokemon::judge_settle(look);
    if (out.faded && quiet_overworld(out.settle, quiet)){
        out.facing = read_facing();
    }
    return out;
}

std::string describe(const FlyLanding& landing){
    return std::format(
        "{}, {}, {}",
        landing.faded ? "faded" : "no fade yet",
        Pokemon::describe(landing.settle),
        landing.facing ? std::string("the player faces ") + Pokemon::direction_name(*landing.facing) : "the player is not readable"
    );
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
