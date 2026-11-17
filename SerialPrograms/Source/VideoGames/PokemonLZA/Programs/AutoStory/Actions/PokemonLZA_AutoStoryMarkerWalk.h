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
#include "PokemonLZA_AutoStoryMarkerArrival.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  On to the objective marker (read_objective_marker()), the camera turned
//  to keep it in the middle (and to find it when it is behind), 90 s at
//  most, until an A prompt (a gate's "Leave", a door's "Enter") or a
//  dialog (a scene that starts on arriving) shows near the objective, as
//  `proof` says (MarkerArrival); one farther off is walked past. When the
//  marker goes first (a scene at the objective takes it away), up to 15 s
//  more for one, if the walk was near. A walk held up (the distance not
//  dropping for 4 reads) steps sideways. Fails as "objective_not_reached"
//  when neither shows near it.
Pokemon::Outcome walk_to_marker(StoryEnv& env, PlzaBackend& backend, MarkerProof proof);

//  Toward the objective marker, steered as walk_to_marker() is, until a
//  distance read taken (MarkerDistance) is `stop_at` or less, or `floor`
//  or less, the marker is lost, or `limit`. It steps only on a read taken:
//  with none, it stands, turns the camera back onto the marker and reads
//  again; still none, it nudges the camera a little, then sidesteps, and
//  past those (MarkerMove::BLOCKED) it ends, logged, without a step on.
//  Held up, it sidesteps too. It walks the lane MarkerDistance::lane()
//  gives, back on the marker's line after each read taken, and ends on
//  the line. Reads below `floor` are never taken. Returns the last read
//  taken; none when none was.
std::optional<int> walk_toward_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, int floor, Milliseconds limit);

//  Back away from the objective marker (the stick down, toward the
//  camera, the camera turned back onto the marker before each step, so its
//  digits keep reading) until a read taken is `stop_at` or more, stepping
//  only on reads taken, sidestepping and ending on the line as
//  walk_toward_marker() does.
std::optional<int> walk_back_from_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, Milliseconds limit);

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
