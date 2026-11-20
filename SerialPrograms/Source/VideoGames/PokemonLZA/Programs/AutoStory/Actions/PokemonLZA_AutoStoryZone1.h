/*  PokemonLZA AutoStory - Wild Zone 1
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure Wild Zone 1 map waypoints, exit routing, zone-membership checks,
 *  and road-patrol start logic.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1_H

#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Target waypoint on the full map.
struct MapWaypoint{
    std::string_view name = {};
    MapPoint pin = {};
    double stop_radius = 0;
};

inline constexpr double CATCH_STOP_RADIUS = 12;

inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 476, .y = 224}};
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 442, .y = 227}, .stop_radius = CATCH_STOP_RADIUS};

//  Returns the waypoint for a catch spot distance in CATCH_SPOTS, or std::nullopt.
std::optional<MapWaypoint> catch_spot_waypoint(int spot);

//  Returns map-pin waypoints from `start` to Wild Zone 1's far exit gate.
std::vector<MapPoint> zone_1_exit_legs(MapPoint start);

//  True if `pin` lies inside Wild Zone 1's bounds (excluding the out-of-zone map center).
bool pin_in_zone_1(MapPoint pin);

//  Confirms via `pin` reads that the player has exited Wild Zone 1.
//  Fails as "in_wild_zone" or "zone_unread".
std::optional<Pokemon::StoryFailure> check_out_of_zone_1(PlzaBackend& backend, MoveOnlyFunction<std::optional<MapPoint>()> pin);

//  Catch patrol spots in Wild Zone 1 (named by their former marker distances).
inline constexpr int CATCH_SPOTS[] = {100, 60};

//  Builds a sequence of `visits` alternating stops cycling through `spots`.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);

//  Parameters for leaving a zone start point onto the search road.
struct ZoneStartRoute{
    MapWaypoint start;
    double start_radius;
    double leg_x;
    double leg_y;
    Milliseconds leg_hold;
};

constexpr ZoneStartRoute TAUNIE_ROCKS_ROUTE{
    .start = ZONE1_ROCKS,
    .start_radius = 8,
    .leg_x = -1,
    .leg_y = 0,
    .leg_hold = Milliseconds(900),
};

//  True if `pin` is present and within `route.start_radius` of `route.start.pin`.
bool at_zone_start(std::optional<MapPoint> pin, const ZoneStartRoute& route);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
