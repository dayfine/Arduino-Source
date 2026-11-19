/*  PokemonLZA AutoStory - Map walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's walks on the console as legs between map waypoints (MapLeg):
 *  the full map read for the player's pin before and after each leg.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMapWalk_H
#define PokemonAutomation_PokemonLZA_AutoStoryMapWalk_H

#include <optional>
#include <span>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryZone1Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PLUS opens the full map, the pin is read (8 looks 250 ms apart), B
//  closes it. None when it does not read.
std::optional<MapPoint> read_pin_on_map(StoryEnv& env, PlzaBackend& backend);

//  A waypoint's last read is the next leg's first. Fails as
//  "leg_off_target", "map_pin_not_read" or "heading_missed".
std::optional<Pokemon::StoryFailure> walk_map_legs(StoryEnv& env, PlzaBackend& backend, std::span<const MapWaypoint> waypoints);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
