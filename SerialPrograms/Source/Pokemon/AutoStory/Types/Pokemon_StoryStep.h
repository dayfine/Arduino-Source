/*  Story Step
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One step of a game's story as plain data.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStep_H
#define PokemonAutomation_Pokemon_StoryStep_H

#include <string>
#include "Pokemon_StoryActionArg.h"
#include "Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One step of a checkpoint: an action and its args.
struct StoryStep{
    StoryStepKind kind = StoryStepKind::TALK;
    std::string name = {};
    std::string why = {};

    //  One of the game's actions of `kind`; empty: the kind's default.
    std::string action = {};
    //  Checked against the action's spec when the step loads.
    StoryActionArgs args = {};

};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
