/*  Story Certainty
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How sure of the player's tile each step must be when it starts, and how
 *  sure it leaves the run. A step that needs more than the steps before it
 *  guarantee walks from a guess: 00.05's route planned open legs from its
 *  save's declared start, a tile the save was never seen on, and walked
 *  into a building (lab-B 2026-11-11 15:22).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryCertainty_H
#define PokemonAutomation_Pokemon_StoryCertainty_H

#include <span>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a step needs at its start and leaves at its end: the action's spec
//  where it says, else its kind's default (a route needs one tile and
//  leaves one; a scene needs nothing and leaves its declared end unseen).
StoryCertainty story_step_certainty(const StoryStep& step, std::span<const StoryActionSpec> specs);

//  How sure the run is after `step` succeeds from `before`.
PoseCertainty certainty_after(const StoryStep& step, PoseEnd leaves, PoseCertainty before);

//  How sure a checkpoint is at its start: SET from its save (its declared
//  start was never seen on the screen), UNKNOWN without one.
PoseCertainty start_certainty(const StoryCheckpoint& data);

//  Each step of `data` that needs a surer pose than the steps before it
//  leave, as "00.05 step 1 to_the_center: needs TILE, has SET".
std::vector<std::string> certainty_gaps(const StoryCheckpoint& data, std::span<const StoryActionSpec> specs);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
