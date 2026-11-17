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
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryZone1Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  PLUS opens the full map, read_map_pin() reads the player's pin (8
//  looks 250 ms apart), B closes it. Logs
//  "[PLZA] map pin: (x, y) heading h", h the minimap arrow's before the
//  map opened, or "[PLZA] map pin: none, heading h".
std::optional<MapPoint> read_pin_on_map(StoryEnv& env, PlzaBackend& backend);

//  Walks to `waypoints` in turn, a MapLeg each: a pin read, then while it
//  is off the waypoint, face_heading() to the leg's heading and the stick
//  held straight on for its hold, and a pin read again. A waypoint's last
//  read is the next leg's first. Fails as "leg_off_target" (wanted and
//  read pins in its detail), "map_pin_not_read" or "heading_missed".
Pokemon::Outcome walk_map_legs(StoryEnv& env, PlzaBackend& backend, std::span<const MapWaypoint> waypoints);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
