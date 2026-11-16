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
#include "Common/Cpp/Time.h"
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

//  Toward the objective marker, steered as walk_to_marker() is, until it is
//  `stop_at` or less away (MarkerDistance: the last read taken, less 3 for
//  each step since it with no read), the marker is lost, or `limit`.
//  Returns where it ended, by that count; none when no distance read.
std::optional<int> walk_toward_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, Milliseconds limit);

//  Back away from the objective marker (the stick down, toward the
//  camera, the camera turned back onto the marker before each step) until
//  it is `stop_at` or more away, counted as walk_toward_marker() counts it.
std::optional<int> walk_back_from_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, Milliseconds limit);

//  The objective marker's distance where the player stands: up to 8 looks,
//  250 ms apart, until MarkerDistance takes a read (two of three agree).
//  None when it takes none: a lone read at the start of 03.08 read 1 with
//  the marker at 120, half behind a rock (console B, 2026-11-16).
std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
