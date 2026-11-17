/*  PokemonLZA AutoStory - Walks between Wild Zone 1's road stops
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's patrol walks between its catch spots on the road (100 and 60)
 *  by the objective marker, as plza/autostory walked them (RoadWalkReads).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryRoadWalk_H
#define PokemonAutomation_PokemonLZA_AutoStoryRoadWalk_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryMarkerDistance.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Walks `direction` (toward the objective marker, or back from it with
//  the stick toward the camera) until a read taken (RoadWalkReads) is at
//  `stop_at`, the marker is lost for 12 camera turns, or `limit`. Before
//  each 0.6 s step the camera turns back onto the marker; a marker pinned
//  to the screen's edge (off screen, no digits) gets a turn and no step.
//  `floor`: as RoadWalkReads takes it. Returns the last read taken; none
//  when none was.
std::optional<int> walk_to_road_stop(
    StoryEnv& env, PlzaBackend& backend,
    MarkerDirection direction, int stop_at, std::optional<int> floor, Milliseconds limit
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
