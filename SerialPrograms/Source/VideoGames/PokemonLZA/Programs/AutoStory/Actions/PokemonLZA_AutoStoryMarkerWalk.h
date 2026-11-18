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

#include <optional>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On to the objective marker (read_objective_marker()), the camera turned
//  to keep it in the middle (and to find it when it is behind), 90 s at
//  most, until an A prompt (a gate's "Leave", a door's "Enter") or a
//  dialog (a scene that starts on arriving) shows near the objective, where
//  the marker's digits may not read (the lab's door up its steps): near
//  with no read over 20 (MarkerProof::NO_FAR_READ); one farther off is
//  walked past. When the marker goes first (a scene at the objective takes
//  it away), up to 15 s more for one, if the walk was near. A walk held up
//  (the distance not dropping for 4 reads) steps sideways. Fails as
//  "objective_not_reached" when neither shows near it.
Pokemon::Outcome walk_to_unread_marker(StoryEnv& env, PlzaBackend& backend);

//  The objective marker's distance where the player stands: up to 8 looks,
//  250 ms apart, until MarkerDistance takes a read (two of three agree,
//  none below `floor`). None when it takes none: the marker half behind a
//  rock read 1, 1 and 4 at the start of 03.08 with the marker at 120
//  (console B, 2026-11-16).
//  `looks`: frames to try; a player standing still can afford more.
std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend, std::optional<int> floor, int looks = 8);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
