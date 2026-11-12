/*  Zone Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include "Pokemon_ZonePose.h"

namespace PokemonAutomation{
namespace Pokemon{


bool pose_matches(const ZonePose& actual, const ZonePose& target){
    return target.zone.empty() || actual.zone == target.zone;
}

std::string pose_text(const ZonePose& pose){
    return std::format("zone {}", (pose.zone.empty() ? std::string("unknown") : pose.zone));
}

double direction_degrees(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return 0;
    case Direction::EAST:
        return 90;
    case Direction::SOUTH:
        return 180;
    case Direction::WEST:
        return -90;
    }
    return 0;
}



} // namespace Pokemon
} // namespace PokemonAutomation
