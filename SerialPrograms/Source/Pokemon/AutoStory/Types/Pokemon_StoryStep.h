/*  Story Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a game's story as plain data: its kind, the action that does
 *  it and the action's args. The shared JSON reader reads it
 *  (Data/Pokemon_StoryStepJson.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_H
#define PokemonAutomation_Pokemon_StoryStep_H

#include <string>
#include "Pokemon_StoryActionArg.h"
#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


// One step of a checkpoint: an action and its args.
struct StoryStep{
    StoryStepKind kind = StoryStepKind::TALK;
    std::string name;
    std::string why;

    //  How the step does its kind: one of the game's actions; for a GAME
    //  step, the game's action. Empty is the kind's default action.
    std::string action;
    //  The arguments the action reads, parsed against its spec when the
    //  game loads the step, with the spec's fallbacks filled in.
    StoryActionArgs args;

};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
