/*  Story Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a game's story, ready to run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_H
#define PokemonAutomation_Pokemon_StoryStep_H

#include <optional>
#include <set>
#include <string>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryBackend.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a run knows; a reload empties it.
struct RunState{
    std::set<std::string> flags = {};
    //  The step under way, "<checkpoint>/<step>", for its log lines.
    std::string step = {};
};

//  A step's action with its args bound.
using StoryStepRun = MoveOnlyFunction<std::optional<StoryFailure>(StoryBackend& backend, RunState& state)>;

struct StoryStep{
    std::string name = {};
    std::string why = {};
    //  The plan's "action": what kind of step it is.
    std::string action = {};
    StoryStepRun run = {};
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
