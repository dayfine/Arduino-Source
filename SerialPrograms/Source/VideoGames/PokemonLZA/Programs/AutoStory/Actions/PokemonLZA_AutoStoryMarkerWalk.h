/*  PokemonLZA AutoStory - Marker walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walking by the main mission's objective marker, where a walk starts
 *  facing whichever way the last scene left the player, so timed legs do
 *  not hold.
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


//  Walks on to the objective marker until an A prompt or a dialog shows near it
//  (MarkerProof::NO_FAR_READ); one farther off is walked past. Fails as
//  "objective_not_reached" when none shows near it.
Pokemon::Outcome walk_to_unread_marker(StoryEnv& env, PlzaBackend& backend);

//  The objective marker's distance once MarkerDistance takes a read within `looks`
//  frames; nullopt when it takes none (a marker half behind a rock misreads).
std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend, std::optional<int> floor, int looks = 8);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
