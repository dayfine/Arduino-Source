/*  Story Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a game hands the story engine: its actions, its save and its
 *  reload, as data plus functions. No subclassing.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryGame_H
#define PokemonAutomation_Pokemon_StoryGame_H

#include <functional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What an action is asked to do: the step it runs (its name and why, for
//  the log and for what an action tunes by step) and the step's args with
//  their fallbacks filled in. Both live for the call only.
struct StoryActionCall{
    const StoryStep& step;
    const StoryActionArgs& args;
};

//  An action's code: reads its call's args and drives the backend. A
//  succeeded action that knows where it left the player returns that pose
//  in its Outcome.
template <StoryPose PoseType>
using StoryActionHandler = std::function<Outcome(StoryBackend<PoseType>& backend, const StoryActionCall& call, RunState& state)>;

//  An action: its spec (kind, name, args) and its code.
template <StoryPose PoseType>
struct StoryAction{
    StoryActionSpec spec = {};
    StoryActionHandler<PoseType> handler = {};
};

//  A game, for the engine.
template <StoryPose PoseType>
struct StoryGame{
    //  "frlg", "bdsp", "plza".
    std::string id = {};
    //  The game's actions. A kind's first is its default, the action of a
    //  step that names none.
    std::vector<StoryAction<PoseType>> actions = {};
    //  The game's save (a wait for an autosave) at the end of `checkpoint`.
    //  Optional: with none, nothing saves.
    std::function<Outcome(StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint)> save = {};
    //  Back to the last save. A succeeded reload that knows where it left
    //  the player returns that pose in its Outcome. Optional: with none, a
    //  failed checkpoint is not retried.
    std::function<Outcome(StoryBackend<PoseType>& backend)> reload = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
