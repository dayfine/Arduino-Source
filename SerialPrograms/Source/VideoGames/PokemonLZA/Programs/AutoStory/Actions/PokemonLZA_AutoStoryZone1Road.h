/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free plan of 03.08's walks along Wild Zone 1's road: the
 *  way from its start save at Taunie's rocks to the road's spot at 100
 *  (ZoneStart: a measured leg off the rocks and its read, then map legs),
 *  and the catch spots the patrol goes between.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H

#include <optional>
#include <span>
#include <vector>
#include "Common/Cpp/Time.h"
#include "PokemonLZA_AutoStoryZone1Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Distances from the objective marker (the far gate): 100, the plaza
//  (Scatterbug), and 60, just before the gate (Mareep). Walking to 100 and
//  back makes the Pokemon at 60 come back when none showed there.
inline constexpr int CATCH_SPOTS[] = {100, 60};

//  How far short of a road stop still counts as there: the walk back to 60
//  stopped at 58 once, and the gate walk from there is steered by the gate.
inline constexpr int ROAD_STOP_SLACK = 3;

//  `visits` stops: {100, 60, 100, ...}.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);


//  A start's way to the search spot: L (camera behind the player), a
//  camera-relative leg whose marker read must fall in [leg_read_min,
//  leg_read_max], then the marker walk for `walk_hold`. `start_radius` is
//  in map pixels around `start`.
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

//  Measured by hand from the 03.08 save: the leg reads 127, then forward
//  reads 102 after 4.8 s; the leg moves the pin 4 to 5 pixels.
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


//  What the way to the search spot does next.
enum class ZoneStartMove{
    WALK_TO_SPOT,
    SEARCH,
    ROCKS_NOT_LEFT,     //  stop: not at the start by the pin, or, the pin unread, the leg's read out of range
    SPOT_NOT_REACHED,   //  stop
};

//  A ZoneStartRoute read by read. A stop stays a stop.
class ZoneStart{
public:
    explicit ZoneStart(const ZoneStartRoute& route)
        : m_route(route)
    {}

    //  `start_pin` (read before the leg) decides; `distance` (after the leg)
    //  only when the pin did not read, as the digits may not read there.
    ZoneStartMove leave(std::optional<MapPoint> start_pin, std::optional<int> distance);

private:
    //  `distance`: the agreed read after the leg, then after the marker walk.
    ZoneStartMove next(std::optional<int> distance);

private:
    const ZoneStartRoute m_route;
    std::optional<ZoneStartMove> m_last;
};


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
