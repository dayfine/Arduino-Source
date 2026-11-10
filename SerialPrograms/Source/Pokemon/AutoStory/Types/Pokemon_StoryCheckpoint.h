/*  Story Checkpoint
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A checkpoint of a tile game's story table as plain data: where its save
 *  leaves the player and its steps in order. build_story_checkpoint()
 *  (Pokemon_StorySteps.h) turns it into engine events.
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


//  A part of a checkpoint, e.g. one floor of a cave: a run of steps that
//  name it in their `section`. Steps are numbered across the checkpoint,
//  not per section.
struct StorySection{
    std::string id;
    std::string title;
    std::vector<std::string> notes;
};

// Milestone row in the story table grouping related steps and starting pose.
struct StoryCheckpoint{
    std::string id;
    std::string title;
    //  Where its starting save leaves the player; none for a new game.
    std::optional<Pose> start;
    std::vector<StoryStep> steps;
    //  The author's notes on the whole checkpoint: the maps, trainers and
    //  numbers its steps were planned from.
    std::vector<std::string> notes = {};
    //  In order; each step's `section` names one of them. Empty when the
    //  checkpoint is not split.
    std::vector<StorySection> sections = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
