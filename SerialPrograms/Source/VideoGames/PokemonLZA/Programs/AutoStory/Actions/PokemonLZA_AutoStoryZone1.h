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
    bool provisional = true;
    double stop_radius = 0;
};

inline constexpr double CATCH_STOP_RADIUS = 12;

inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 476, .y = 224}, .provisional = false};
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 442, .y = 227}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};

//  Returns the waypoint for a catch spot distance in CATCH_SPOTS, or std::nullopt.
std::optional<MapWaypoint> catch_spot_waypoint(int spot);

//  Returns map-pin waypoints from `start` to Wild Zone 1's far exit gate.
std::vector<MapPoint> zone_1_exit_legs(MapPoint start);

//  True if `pin` lies inside Wild Zone 1's bounds (excluding the out-of-zone map center).
bool pin_in_zone_1(MapPoint pin);

//  Confirms via `pin` reads that the player has exited Wild Zone 1.
//  Fails as "in_wild_zone" or "zone_unread".
std::optional<Pokemon::StoryFailure> check_out_of_zone_1(PlzaBackend& backend, MoveOnlyFunction<std::optional<MapPoint>()> pin);

//  Objective-marker distances of Wild Zone 1's alternating catch patrol spots.
inline constexpr int CATCH_SPOTS[] = {100, 60};

//  Tolerance in marker units for considering a road stop reached.
inline constexpr int ROAD_STOP_SLACK = 3;

//  Builds a sequence of `visits` alternating stops cycling through `spots`.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);

//  Parameters for leaving a zone start point onto the search road.
struct ZoneStartRoute{
    MapWaypoint start;
    double start_radius;
    double leg_x;
    double leg_y;
    Milliseconds leg_hold;
    int leg_read_min;
    int leg_read_max;
    Milliseconds walk_hold;
};

constexpr ZoneStartRoute TAUNIE_ROCKS_ROUTE{
    .start = ZONE1_ROCKS,
    .start_radius = 8,
    .leg_x = -1,
    .leg_y = 0,
    .leg_hold = Milliseconds(900),
    .leg_read_min = 120,
    .leg_read_max = 135,
    .walk_hold = Milliseconds(5600),
};

//  Next decision from ZoneStart::leave().
enum class ZoneStartMove{
    WALK_TO_SPOT,
    SEARCH,
    ROCKS_NOT_LEFT,
    SPOT_NOT_REACHED,
};

//  Stateful route checker for leaving a zone start point; terminal stops remain sticky.
class ZoneStart{
public:
    explicit ZoneStart(const ZoneStartRoute& route)
        : m_route(route)
    {}

    //  Decides next move from `start_pin` (preferred) or post-leg marker `distance`.
    ZoneStartMove leave(std::optional<MapPoint> start_pin, std::optional<int> distance);

private:
    ZoneStartMove next(std::optional<int> distance);

    const ZoneStartRoute m_route;
    std::optional<ZoneStartMove> m_last;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
