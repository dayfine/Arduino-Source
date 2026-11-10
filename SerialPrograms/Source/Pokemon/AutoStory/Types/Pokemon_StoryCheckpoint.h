/*  Story Checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint of a tile game's story table as plain data: where its save
 *  leaves the player and its steps in order. build_story_checkpoint()
 *  (Pokemon_StoryStepExecutor.h) turns it into engine events.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCheckpoint_H
#define PokemonAutomation_Pokemon_StoryCheckpoint_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon_StoryStep.h"

namespace PokemonAutomation{
namespace Pokemon{


// Milestone row in the story table grouping related steps and starting pose.
struct StoryCheckpoint{
    std::string id;
    std::string title;
    //  Where its starting save leaves the player; none for a new game.
    std::optional<Pose> start;
    std::vector<StoryStep> steps;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
