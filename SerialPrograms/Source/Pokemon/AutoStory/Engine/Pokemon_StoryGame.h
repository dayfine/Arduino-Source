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
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "Pokemon_StoryBackend.h"

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

using StoryActionHandler = MoveOnlyFunction<
    std::optional<StoryFailure>(StoryBackend& backend, const StoryActionCall& call, RunState& state)
>;

struct StoryAction{
    StoryActionSpec spec = {};
    StoryActionHandler handler = {};
};

struct StoryGame{
    //  "frlg", "bdsp", "plza".
    std::string id = {};
    //  A kind's first action is the one a step that names none runs.
    std::vector<StoryAction> actions = {};
    //  Called after each passed checkpoint; a failure is logged. None: nothing saves.
    MoveOnlyFunction<std::optional<StoryFailure>(StoryBackend& backend, const StoryCheckpoint& passed)> save = {};
    //  Reloads after `failed` failed; returns the id of the checkpoint whose
    //  start the game is now at, none when it could not reload or cannot tell.
    //  None: a failed checkpoint is not retried.
    MoveOnlyFunction<std::optional<std::string>(StoryBackend& backend, const StoryCheckpoint& failed)> reload = {};
    //  Runs of one checkpoint, counting the first.
    size_t attempts_per_checkpoint = 1;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
