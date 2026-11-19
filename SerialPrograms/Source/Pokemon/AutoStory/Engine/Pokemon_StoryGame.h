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
#include <set>
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


//  What a run knows; a reload empties it.
struct RunState{
    std::set<std::string> flags = {};
};

//  The step an action runs and its args with fallbacks filled in; both live
//  for the call only.
struct StoryActionCall{
    const StoryStep& step;
    const StoryActionArgs& args;
};

//  An action's code.
using StoryActionHandler = std::function<Outcome(StoryBackend& backend, const StoryActionCall& call, RunState& state)>;

//  An action: its spec and its code.
struct StoryAction{
    StoryActionSpec spec = {};
    StoryActionHandler handler = {};
};

//  What a game hands the story engine.
struct StoryGame{
    //  "frlg", "bdsp", "plza".
    std::string id = {};
    //  A kind's first action is the one a step that names none runs.
    std::vector<StoryAction> actions = {};
    //  None: nothing saves.
    StorySavePolicy save = {};
    //  Back to the last save. None: a failed checkpoint is not retried.
    std::function<Outcome(StoryBackend& backend)> reload = {};
    //  Runs of a checkpoint, counting the first, while a reload returns to
    //  its start.
    size_t attempts_per_checkpoint = 1;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
