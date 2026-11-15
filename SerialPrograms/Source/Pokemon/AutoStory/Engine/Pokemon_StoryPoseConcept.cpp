/*  Story Pose Concept
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <string>
#include "Pokemon_StoryPoseConcept.h"

namespace PokemonAutomation{
namespace Pokemon{


bool pose_matches(const GridPose& actual, const GridPose& target){
    if (actual.map != target.map){
        return false;
    }
    if (target.facing && actual.facing != target.facing){
        return false;
    }
    if (target.tiles.empty()){
        return true;
    }
    return !actual.tiles.empty() && std::all_of(actual.tiles.begin(), actual.tiles.end(), [&](const TilePos& tile){
        return std::find(target.tiles.begin(), target.tiles.end(), tile) != target.tiles.end();
    });
}

std::string pose_text(const GridPose& pose){
    std::string text = pose.map;
    for (const TilePos& tile : pose.tiles){
        text += std::format(" {}", to_string(tile));
    }
    if (pose.facing){
        text += std::format(" facing {}", direction_name(*pose.facing));
    }
    return text;
}



} // namespace Pokemon
} // namespace PokemonAutomation
