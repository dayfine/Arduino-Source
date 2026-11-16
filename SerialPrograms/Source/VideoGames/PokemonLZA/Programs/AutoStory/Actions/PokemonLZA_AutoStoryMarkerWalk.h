/*  PokemonLZA AutoStory - Marker walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walking by the main mission's objective marker, where a walk starts
 *  facing whichever way the last scene left the player (out of a wild
 *  zone's gate), so timed legs do not hold (03.09, console B 2026-11-10).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMarkerWalk_H
#define PokemonAutomation_PokemonLZA_AutoStoryMarkerWalk_H

#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On to the objective marker (read_objective_marker()), the camera turned
//  to keep it in the middle (and to find it when it is behind), 90 s at
//  most, until an A prompt (a gate's "Leave", a door's "Enter") or a
//  dialog (a scene that starts on arriving) shows; then up to 15 s more for
//  one, since a scene at the objective takes the marker away first. A
//  walk held up (the distance not dropping for 4 reads) steps sideways.
//  Fails as "objective_not_reached" when neither shows.
Pokemon::Outcome walk_to_marker(StoryEnv& env, PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
