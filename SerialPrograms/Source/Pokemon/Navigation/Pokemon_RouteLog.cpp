/*  Route Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_RouteLog.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string to_string(const RouteTile& tile){
    return std::format("{} {}", tile.map, to_string(tile.pos));
}

std::string describe(const std::vector<RouteTile>& tiles){
    if (tiles.empty()){
        return "an unknown tile";
    }
    std::string out;
    for (const RouteTile& tile : tiles){
        out += (out.empty() ? "" : ", ") + to_string(tile);
    }
    return out;
}

std::string to_string(const RouteLogLeg& leg){
    const std::string direction = direction_name(leg.direction);
    switch (leg.action){
    case RouteLegAction::SURF:
        return std::format("surf {}", direction);
    case RouteLegAction::FACE:
        return std::format("face {}", direction);
    case RouteLegAction::OPEN_GATE:
        return std::format("open gate {}", direction);
    case RouteLegAction::PRESS_SWITCH:
        return std::format("press switch {}", direction);
    case RouteLegAction::WALK:
        break;
    }
    const std::string_view end = leg.end == RouteLegEnd::WALL ? " (to wall)" : leg.end == RouteLegEnd::WARP ? " (warp)" : " (open)";
    return std::format("{} {}{}", direction, leg.tiles, end);
}

std::string basis_name(EndBasis basis){
    switch (basis){
    case EndBasis::WALKED:
        return "walked";
    case EndBasis::ASSUMED:
        return "assumed";
    case EndBasis::UNKNOWN:
        return "unknown";
    }
    return "?";
}


//  "<leg> to <tiles>", as the plan and the leg lines both write a leg.
static std::string planned_leg_text(const PlannedRouteLeg& leg){
    return std::format("{} to {}", to_string(leg.leg), describe(leg.after));
}

std::string route_plan_line(const std::string& label, const std::vector<RouteTile>& from, const std::vector<PlannedRouteLeg>& legs){
    std::string text = std::format("{}: plan from {}:", label, describe(from));
    for (size_t i = 0; i < legs.size(); i++){
        text += (i == 0 ? " " : "; ") + planned_leg_text(legs[i]);
    }
    return text;
}

std::string route_leg_line(size_t number, size_t count, const PlannedRouteLeg& leg){
    return std::format("Leg {}/{}: {}", number, count, planned_leg_text(leg));
}

std::string route_leg_end_line(
    const std::string& label, const RouteLogLeg& leg,
    const std::vector<RouteTile>& at, EndBasis basis, const std::string& why
){
    const std::string how = why.empty() ? basis_name(basis) : std::format("{}: {}", basis_name(basis), why);
    return std::format("{}: {}: ended at {} ({}).", label, to_string(leg), describe(at), how);
}

std::string route_replan_line(
    const std::string& label, const RouteLogLeg& leg,
    const std::string& what, const std::vector<RouteTile>& at
){
    return std::format("{}: {}: {}, at {}; planning again.", label, to_string(leg), what, describe(at));
}

std::string route_found_line(const std::string& label, const std::vector<RouteTile>& at, size_t holds){
    return std::format("{}: found again at {}, after {} hold(s).", label, describe(at), holds);
}

std::string route_trainer_line(const std::string& label, const std::string& who, const std::vector<RouteTile>& at){
    return std::format("{}: {} stopped us about {}.", label, who, describe(at));
}

std::string route_arrived_line(const std::string& label, const std::vector<RouteTile>& at){
    return std::format("{}: arrived at {}.", label, describe(at));
}



} // namespace Pokemon
} // namespace PokemonAutomation
