/*  PokemonLZA AutoStory - World walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Console-backed map-pin waypoint walking and objective-marker walking.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryWorldWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryWorldWalks_H

#include <optional>
#include <span>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryConsoleBackend.h"
#include "PokemonLZA_AutoStoryZone1.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Opens the full map, reads the player pin, and closes the map; returns std::nullopt if unread.
std::optional<MapPoint> read_pin_on_map(StoryEnv& env, PlzaBackend& backend);

//  Walks sequentially through `waypoints` using full-map pin feedback.
//  Fails as "leg_off_target", "map_pin_not_read", or "heading_missed".
std::optional<Pokemon::StoryFailure> walk_map_legs(StoryEnv& env, PlzaBackend& backend, std::span<const MapWaypoint> waypoints);

//  Walks toward the main-mission objective marker until an interact prompt or dialog appears near it.
//  Fails as "objective_not_reached" if arrival is not detected.
std::optional<Pokemon::StoryFailure> walk_to_unread_marker(StoryEnv& env, PlzaBackend& backend);

//  Reads the objective marker distance within `looks` frames using optional `floor`; returns std::nullopt if unread.
std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend, std::optional<int> floor, int looks = 8);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
