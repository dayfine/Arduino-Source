/*  Zone Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "Pokemon_ZonePose.h"

namespace PokemonAutomation{
namespace Pokemon{


bool pose_matches(const ZonePose& actual, const ZonePose& target){
    return target.zone.empty() || actual.zone == target.zone;
}

std::string pose_text(const ZonePose& pose){
    return "zone " + (pose.zone.empty() ? std::string("unknown") : pose.zone);
}



} // namespace Pokemon
} // namespace PokemonAutomation
