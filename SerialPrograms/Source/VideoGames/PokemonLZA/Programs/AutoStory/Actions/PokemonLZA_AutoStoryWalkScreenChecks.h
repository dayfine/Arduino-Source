/*  PokemonLZA AutoStory - Walk screen checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a story walk (run_story_move(), PokemonLZA_AutoStoryMoves.h) reads
 *  of the screen, since the walk itself reads nothing: before it, that the
 *  player has free control (check_can_walk()); during its last leg, the
 *  fade it ends on (walk_until()).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWalkScreenChecks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWalkScreenChecks_H

#include <string_view>
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryObservation.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Fails as "walk_not_free" unless two looks 150 ms apart show no box,
//  choice, fade or battle: a walk started anywhere else walks blind. The
//  tutorial has no party bar, so it is not required.
Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk);

//  Walks `move`, holding its last leg in slices of 250 ms until a look
//  shows `end` (a fade), the leg's time as the upper bound. Fails as
//  "walk_no_end" if `end` does not show within it. `heading` reads the
//  compass (face_heading()).
Pokemon::Outcome walk_until(
    PlzaBackend& backend, std::string_view walk, const StoryMove& move, const PlzaReader& heading,
    PokemonAutomation::ScreenEvent end
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
