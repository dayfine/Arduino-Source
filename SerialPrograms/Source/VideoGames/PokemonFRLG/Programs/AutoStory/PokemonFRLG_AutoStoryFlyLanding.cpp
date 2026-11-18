/*  AutoStory Fly Landing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "PokemonFRLG_AutoStoryFlyLanding.h"
#include "PokemonFRLG_AutoStoryQuietOverworld.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Measured on Fuchsia to Pallet Town (console B, 2026-11-12 23:59 and
//  2026-11-13 02:46; Test_AutoStoryFlyLandingFlyOff): Fuchsia with the
//  Fly's bird over it differs from Fuchsia before the Fly by 32, Pallet
//  fading in by 173 and Pallet with the player standing by 149. The bar
//  sits between them.
constexpr double FLY_LEFT_RMSD = 80.0;

double departure_change(const ImageViewRGB32& departure, const ImageViewRGB32& now, const ImageFloatBox& game){
    return ImageMatch::pixel_RMSD(extract_box_reference(departure, game), extract_box_reference(now, game));
}

bool left_departure(const ImageViewRGB32& departure, const ImageViewRGB32& now, const ImageFloatBox& game){
    return departure_change(departure, now, game) > FLY_LEFT_RMSD;
}

bool fly_faded(const Pokemon::ScreenLook& look){
    for (const ScreenEventChange& change : look.log.changes()){
        const bool fade = change.event == ScreenEvent::FADE_BLACK || change.event == ScreenEvent::FADE_WHITE;
        if (fade && change.edge == ScreenEventEdge::ROSE){
            return true;
        }
    }
    return false;
}

bool FlyLanding::landed() const{
    return faded && left && facing.has_value();
}

FlyLanding judge_fly_landing(
    const Pokemon::ScreenLook& look, Milliseconds quiet,
    const std::function<bool()>& has_left,
    const std::function<std::optional<Pokemon::Direction>()>& read_facing
){
    FlyLanding out;
    out.faded = fly_faded(look);
    out.settle = Pokemon::judge_settle(look);
    if (out.faded && quiet_overworld(out.settle, quiet)){
        out.left = has_left();
    }
    if (out.left){
        out.facing = read_facing();
    }
    return out;
}

std::string describe(const FlyLanding& landing){
    return std::format(
        "{}, {}, {}, {}",
        landing.faded ? "faded" : "no fade yet",
        Pokemon::describe(landing.settle),
        landing.left ? "left the departure town" : "not left the departure town",
        landing.facing ? std::string("the player faces ") + Pokemon::direction_name(*landing.facing) : "the player is not readable"
    );
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
