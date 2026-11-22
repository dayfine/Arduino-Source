/*  BDSP AutoStory - Story checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Direction.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


std::string format_grid_pose(const Pokemon::GridPose& pose){
    if (pose.map.empty()){
        return "unknown";
    }
    std::string out = pose.map;
    for (const Pokemon::TilePos& tile : pose.tiles){
        out += std::format(" ({}, {})", tile.x, tile.y);
    }
    if (pose.facing){
        out += std::format(" facing {}", Pokemon::direction_name(*pose.facing));
    }
    if (pose.tiles.empty()){
        out += " (map)";
    }else if (pose.tiles.size() == 1){
        out += " (tile)";
    }else{
        out += " (set)";
    }
    return out;
}

std::optional<RobustPlanCosts> story_route_costs(const std::string& variant){
    if (variant.empty() || variant == ROUTE_ONTO_TRIGGER || variant == ROUTE_INTO_SCENE || variant == "route"){
        return route_plan_costs();
    }
    if (variant == "town"){
        return town_plan_costs();
    }
    if (variant == "robust"){
        return RobustPlanCosts();
    }
    if (variant == "line"){
        return waypoint_plan_costs();
    }
    return std::nullopt;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
