/*  Story Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a game hands the story engine: its actions, save, reload and retries.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryGame_H
#define PokemonAutomation_Pokemon_StoryGame_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryBackend.h"
#include "Pokemon_StorySavePolicy.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The step an action runs and its args with fallbacks filled in; both live
//  for the call only.
struct StoryActionCall{
    const StoryStep& step;
    const StoryActionArgs& args;
};

//  An action's code. A succeeded action that knows where it left the player
//  returns that pose in its Outcome.
template <StoryPose PoseType>
using StoryActionHandler = std::function<Outcome(StoryBackend<PoseType>& backend, const StoryActionCall& call, RunState& state)>;

//  An action: its spec and its code.
template <StoryPose PoseType>
struct StoryAction{
    StoryActionSpec spec = {};
    StoryActionHandler<PoseType> handler = {};
};

//  What a game hands the story engine.
template <StoryPose PoseType>
struct StoryGame{
    //  "frlg", "bdsp", "plza".
    std::string id = {};
    //  A kind's first action is the one a step that names none runs.
    std::vector<StoryAction<PoseType>> actions = {};
    //  None: nothing saves.
    StorySavePolicy<PoseType> save = {};
    //  Back to the last save; may return the pose it left the player in.
    //  None: a failed checkpoint is not retried.
    std::function<Outcome(StoryBackend<PoseType>& backend)> reload = {};
    //  Runs of a checkpoint, counting the first, while a reload returns to
    //  its start.
    size_t attempts_per_checkpoint = 1;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
