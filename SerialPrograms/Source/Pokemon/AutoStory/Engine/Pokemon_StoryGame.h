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
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_StoryCheckpointTypes.h"
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
using StoryActionHandler = std::function<Outcome(StoryBackend<PoseType>& backend, RunState& state, const StoryActionCall& call)>;

//  Fills an observation's pose or readings from what the backend saw: a
//  console reads the frame (OCR, templates), a simulation its model.
template <StoryPose PoseType>
using StoryReader = std::function<void(StoryBackend<PoseType>& backend, Observation<PoseType>& observation)>;

//  When the engine runs a reader.
enum class StoryReaderUse{
    //  Only for a step's skip_if condition of its name: it is slow, or
    //  needs a screen a checkpoint need not end on (PLZA's heading waits
    //  for the minimap and throws without it).
    ON_REQUEST,
    //  Also after a checkpoint whose postconditions need what it supplies:
    //  it is cheap and safe on any screen a checkpoint ends on.
    AFTER_CHECKPOINT,
};

//  What the engine reads the game with. A reader whose name is a step's
//  skip_if condition answers it: the condition holds when the reader sets
//  a non-zero reading of that name.
template <StoryPose PoseType>
struct StoryGameReader{
    std::string name = {};
    //  What it fills: "pose" (Observation::pose), a reading ("heading"), or
    //  every reading with a prefix ("bag.*", "flag.*").
    std::vector<std::string> supplies = {};
    //  Opt in to the postcondition check with AFTER_CHECKPOINT.
    StoryReaderUse use = StoryReaderUse::ON_REQUEST;
    //  It reads a console's screen: never run on another backend.
    bool console_only = false;
    StoryReader<PoseType> read = {};
};

//  Whether `supplies` (StoryGameReader::supplies) covers `need`: the same
//  name, or a prefix ending in "*" that `need` starts with.
inline bool story_reader_supplies(const std::vector<std::string>& supplies, std::string_view need){
    for (const std::string& each : supplies){
        if (each == need){
            return true;
        }
        if (each.ends_with('*') && need.starts_with(std::string_view(each).substr(0, each.size() - 1))){
            return true;
        }
    }
    return false;
}

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
    //  "pose", "bag", "heading", or a skip_if condition.
    std::vector<StoryGameReader<PoseType>> readers = {};
    //  The game's save (the menu, or a wait for an autosave) at the end of
    //  `checkpoint`, or at its SAVE step. The engine decides whether to call
    //  it (Pokemon_StoryEnginePolicy.h).
    std::function<Outcome(StoryBackend<PoseType>& backend, const StoryCheckpoint& checkpoint)> save = {};
    //  Back to the last save. A succeeded reload that knows where it left
    //  the player returns that pose in its Outcome.
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
