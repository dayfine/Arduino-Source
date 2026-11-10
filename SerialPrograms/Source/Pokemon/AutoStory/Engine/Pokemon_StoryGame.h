/*  Story Game
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a game hands the story engine: its actions, its readers and its
 *  save, as data plus functions. No subclassing. The engine registers the
 *  shared built-in actions first (Pokemon_StoryBuiltinActions.h); a game's
 *  action with the same kind and name replaces the built-in one.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryGame_H
#define PokemonAutomation_Pokemon_StoryGame_H

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryCheckpointTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionSpec.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


//  An action's code: reads its args (fallbacks already filled in) and
//  drives the backend. A succeeded action that knows where it left the
//  player returns that pose in its Outcome.
template <StoryPose PoseType>
using StoryActionHandler = std::function<Outcome(StoryBackend<PoseType>& backend, RunState& state, const StoryActionArgs& args)>;

//  Fills an observation's pose or readings from what the backend saw: a
//  console reads the frame (OCR, templates), a simulation its model. A
//  reader whose name is a step's skip_if condition answers it: the
//  condition holds when the reader sets a non-zero reading of that name.
template <StoryPose PoseType>
using StoryReader = std::function<void(StoryBackend<PoseType>& backend, Observation<PoseType>& observation)>;

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
    //  The game's own actions, and its replacements of built-in ones.
    std::vector<StoryAction<PoseType>> actions = {};
    //  By name: "pose", "bag", "facing", or a skip_if condition.
    std::vector<std::pair<std::string, StoryReader<PoseType>>> readers = {};
    //  The game's save (the menu, or a wait for an autosave). The engine
    //  decides whether to call it (Pokemon_StoryEnginePolicy.h).
    std::function<Outcome(StoryBackend<PoseType>& backend)> save = {};
    //  Back to the last save.
    std::function<Outcome(StoryBackend<PoseType>& backend)> reload = {};
    //  Called as each attempt at a checkpoint starts, to reset what the game
    //  keeps per attempt (trainers who can see us again). Optional.
    std::function<void(const StoryCheckpoint& checkpoint)> begin_attempt = {};
    //  What can stop a step part-way (a wild battle, a trainer who saw
    //  us), built for each checkpoint run. Optional.
    std::function<std::vector<Interrupt>(StoryBackend<PoseType>& backend)> interrupts = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
