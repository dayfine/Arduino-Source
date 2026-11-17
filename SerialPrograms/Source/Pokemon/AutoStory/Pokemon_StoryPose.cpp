/*  Story Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryPose.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string certainty_name(PoseCertainty certainty){
    switch (certainty){
    case PoseCertainty::TILE:
        return "tile";
    case PoseCertainty::COLUMN:
        return "column";
    case PoseCertainty::ROW:
        return "row";
    case PoseCertainty::SET:
        return "set";
    case PoseCertainty::UNKNOWN:
        return "unknown";
    }
    return "?";
}

bool certainty_covers(PoseCertainty have, PoseCertainty need){
    switch (need){
    case PoseCertainty::UNKNOWN:
        return true;
    case PoseCertainty::SET:
        return have != PoseCertainty::UNKNOWN;
    case PoseCertainty::COLUMN:
    case PoseCertainty::ROW:
        return have == need || have == PoseCertainty::TILE;
    case PoseCertainty::TILE:
        return have == PoseCertainty::TILE;
    }
    return false;
}

std::string to_string(const Pose& pose){
    std::string text = pose.map.empty() ? "?" : pose.map;
    if (pose.where){
        const std::string where = pose.where->describe();
        if (!where.empty()){
            text += std::format(" {}", where);
        }
    }
    return std::format("{} ({})", text, certainty_name(pose.certainty));
}



} // namespace Pokemon
} // namespace PokemonAutomation
