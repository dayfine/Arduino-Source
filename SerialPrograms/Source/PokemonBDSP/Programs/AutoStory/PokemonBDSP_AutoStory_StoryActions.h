/*  BDSP AutoStory - Story actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's story steps as one action table (Pokemon_StoryActions.h), which
 *  the console (StoryExecutor) and the simulation (SimExecutor) each bind.
 *  The walking steps (ROUTE, LEGS, STEP, TALK, MASH) are the same code on
 *  both: StoryWalkSteps on the binding's StoryStepIO. The steps that need
 *  the console (the intro, the options, answers, battles, training, the
 *  Center, saves, and the story's events, story_events()) are the
 *  binding's own: played on the console, stood in for on the simulation.
 *
 *  A GAME step runs only the event its variant names, and BDSP has no SHOP
 *  or READY step: lint_story_checkpoint() reports a step with neither.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryActions_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryActions_H

#include <functional>
#include <string>
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The actions a binding does its own way, one per kind that needs the
//  console.
struct StoryBoundActions{
    Pokemon::StoryActionFunction new_game = {};
    Pokemon::StoryActionFunction options = {};
    Pokemon::StoryActionFunction answer = {};
    Pokemon::StoryActionFunction battle = {};
    Pokemon::StoryActionFunction train = {};
    Pokemon::StoryActionFunction heal = {};
    Pokemon::StoryActionFunction save = {};
    //  The action for the story event named `name` (story_events()).
    std::function<Pokemon::StoryActionFunction(const std::string& name)> event = {};
};

//  BDSP's story actions: the walking steps on `walks`, the rest `bound`'s.
//  `walks` must outlive the table.
Pokemon::StoryActionTable story_action_table(StoryWalkSteps& walks, const StoryBoundActions& bound);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
