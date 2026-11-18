/*  Overworld Route Formatting
 *  From: https://github.com/PokemonAutomation/
 */
#include <format>
#include "PokemonFRLG_OverworldRoute.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
std::string to_string(const MapPosition& position){
    return position.map + " " + to_string(position.point);
}

std::string to_string(const RouteStep& step){
    std::string text;
    switch (step.action){
    case RouteAction::WALK:
        text = std::format("walk {} {}", direction_name(step.direction), step.tiles);
        if (step.warps){
            text += " (warp)";
        }
        if (step.to_wall){
            text += " (to wall)";
        }
        if (step.spin_tiles){
            text += std::format(" (spins {})", *step.spin_tiles);
        }
        if (step.lands_after > 0){
            text += std::format(" (lands after {})", step.lands_after);
        }
        break;
    case RouteAction::SURF:
        text = "surf " + std::string(direction_name(step.direction));
        break;
    case RouteAction::FACE:
        text = "face " + std::string(direction_name(step.direction));
        break;
    case RouteAction::OPEN_GATE:
        text = "open gate " + std::string(direction_name(step.direction));
        break;
    case RouteAction::PRESS_SWITCH:
        text = "press switch " + std::string(direction_name(step.direction));
        break;
    }
    return text + " -> " + to_string(step.end);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
