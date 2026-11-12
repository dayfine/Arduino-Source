/*  PokemonLZA AutoStory - Step validator
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's checks of a story step's args beyond their types, which the
 *  engine's lint (StoryActionRegistry::lint) checks: a walk's legs, its end
 *  fade, wait and heading, and approach_starter's walks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStepValidator_H
#define PokemonAutomation_PokemonLZA_AutoStoryStepValidator_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The starters, in the order approach_starter's args list them.
inline constexpr StarterChoice STARTER_CHOICES[] = {StarterChoice::CHIKORITA, StarterChoice::TEPIG, StarterChoice::TOTODILE};

//  approach_starter's arg for `starter`: the walk to it ("chikorita").
std::string_view starter_key(StarterChoice starter);

//  A walk's "end" arg as the event it ends on: "fade_white" or
//  "fade_black"; nothing for any other name.
std::optional<PokemonAutomation::ScreenEvent> walk_end_event(std::string_view end);

//  What is wrong with the args of a "walk" or "approach_starter" step:
//  each leg a [turn, ms] pair, turning -180 to 180 degrees and walking 0 ms
//  or more, some leg walking; an end a fade, after a leg that walks; a
//  heading 0 to 359. Nothing for any other step.
std::vector<std::string> plza_step_arg_errors(const Pokemon::StoryStep& step);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
