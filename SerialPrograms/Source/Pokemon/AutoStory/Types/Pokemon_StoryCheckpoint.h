/*  Story Checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint of a game's story table as plain data: where its save
 *  leaves the player and its steps in order.
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


// Milestone row in the story table grouping related steps and starting pose.
struct StoryCheckpoint{
    std::string id;
    std::string title;
    //  Where its starting save leaves the player; none for a new game.
    std::optional<DeclaredPose> start;
    std::vector<StoryStep> steps;
    //  Its phase and verification (JSON "metadata"), which decide when a
    //  run of it may save.
    StoryCheckpointMetadata metadata = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
