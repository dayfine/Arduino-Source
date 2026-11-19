/*  Story Save Policy
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether a run is on a save made at a passed checkpoint's end.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySavePolicy_H
#define PokemonAutomation_Pokemon_StorySavePolicy_H

#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


//  The failure of a policy's answer when the run is not on a save: no error.
inline constexpr std::string_view NOT_SAVED = "not_saved";
//  NOT_SAVED with `reason` as its detail.
inline Outcome outcome_not_saved(std::string reason){
    return outcome_failed(std::string(NOT_SAVED), std::move(reason));
}

//  Saves at the end of `checkpoint`, which passed, if the policy saves: ok
//  when the run is on that save, outcome_not_saved() when not, another
//  failure when a save failed.
using StorySavePolicy = std::function<Outcome(StoryBackend& backend, const StoryCheckpoint& checkpoint)>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
