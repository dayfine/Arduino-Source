/*  Story Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryPose.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string to_string(const Pose& pose){
    std::string text = pose.map.empty() ? "?" : pose.map;
    if (pose.where){
        const std::string where = pose.where->describe();
        if (!where.empty()){
            text += std::format(" {}", where);
        }
    }
    return text;
}



} // namespace Pokemon
} // namespace PokemonAutomation
