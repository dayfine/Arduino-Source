/*  Story Action Registry
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks and runs a story step with its game's action.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActionRegistry_H
#define PokemonAutomation_Pokemon_StoryActionRegistry_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryGame.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What is wrong with `checkpoint` before it runs: no id, empty or repeated
//  step names, actions `game` does not have, args that don't fit a spec.
std::vector<std::string> lint_story_checkpoint(const StoryGame& game, const StoryCheckpoint& checkpoint);

//  Runs `step`'s action with its args' fallbacks filled in, then logs a
//  "[Step]" line. A step with no action fails before any input.
std::optional<StoryFailure> run_story_step(StoryBackend& backend, const StoryStep& step, StoryGame& game, RunState& state);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
