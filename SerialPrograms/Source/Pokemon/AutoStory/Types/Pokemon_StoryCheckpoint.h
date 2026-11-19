/*  Story Checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint of a game's story as plain data.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpoint_H
#define PokemonAutomation_Pokemon_StoryCheckpoint_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon_DeclaredPose.h"
#include "Pokemon_StoryCheckpointMetadata.h"
#include "Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The stretch of story between two saves: its start and its steps.
struct StoryCheckpoint{
    std::string id = {};
    std::string title = {};
    //  Where its starting save leaves the player; none for a new game.
    std::optional<DeclaredPose> start = {};
    std::vector<StoryStep> steps = {};
    StoryCheckpointMetadata metadata = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
