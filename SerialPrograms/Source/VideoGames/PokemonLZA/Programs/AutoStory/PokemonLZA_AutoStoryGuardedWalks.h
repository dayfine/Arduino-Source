/*  PokemonLZA AutoStory - Guarded walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The checks around a story walk (PokemonLZA_AutoStoryMoves.h), which
 *  itself reads nothing: it starts only on a screen with no box, choice,
 *  fade or battle, and a walk that ends on a fade stops on it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGuardedWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryGuardedWalks_H

#include <string_view>
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "PokemonLZA_AutoStoryObservation.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Fails as "walk_not_free" unless two looks 150 ms apart show no box,
//  choice, fade or battle: a walk started anywhere else walks blind. The
//  tutorial has no party bar, so it is not required.
Pokemon::Outcome check_can_walk(PlzaStoryBackend& backend, std::string_view walk);

//  Walks `move`, holding its last leg in slices of 250 ms until a look
//  shows `end` (a fade), the leg's time as the upper bound. Fails as
//  "walk_no_end" if `end` does not show within it.
Pokemon::Outcome walk_until(
    PlzaStoryBackend& backend, std::string_view walk, const StoryMove& move, PokemonAutomation::ScreenEvent end
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
