/*  PokemonLZA AutoStory - Walk screen checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a story walk (run_story_move(), PokemonLZA_AutoStoryMoves.h) reads
 *  of the screen, since the walk itself reads nothing: before it, that the
 *  player has free control (check_can_walk()); during its last leg, the
 *  event it ends on (walk_until()); after it, where it stopped
 *  (finish_walk()).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWalkScreenChecks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWalkScreenChecks_H

#include <string_view>
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Fails as "walk_not_free" unless, within 15 s, two looks in a row 250 ms
//  apart show no advance arrow, dialog box or fade: a walk started anywhere
//  else walks blind.
Pokemon::Outcome check_can_walk(PlzaBackend& backend, std::string_view walk);

//  After a walk's legs with no end event: one look, which on a console
//  waits for the legs' inputs to run (observe()), so the step ends where
//  the walk does, not when its inputs are queued. Logs what the screen
//  shows there.
void finish_walk(PlzaBackend& backend, std::string_view walk);

//  Walks `move`, holding its last leg in slices of 250 ms until a look
//  shows `end` (a fade, or the box of a talk the walk runs into), the
//  leg's time as the upper bound. A black fade ends it too: the scene at
//  the walk's end takes over. Fails as "walk_no_end" if neither shows
//  within it.
Pokemon::Outcome walk_until(PlzaBackend& backend, std::string_view walk, const StoryMove& move, ScreenEvent end);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
