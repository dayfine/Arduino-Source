/*  Story Checkpoint Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StoryCheckpointTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


void apply_interrupt_pose(InterruptPose effect, Pose& pose){
    switch (effect){
    case InterruptPose::UNCHANGED:
        return;
    case InterruptPose::WITHIN_SET:
        if (pose.certainty != PoseCertainty::UNKNOWN){
            pose.certainty = PoseCertainty::SET;
        }
        return;
    case InterruptPose::LOST:
        pose.where.reset();
        pose.certainty = PoseCertainty::UNKNOWN;
        return;
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
