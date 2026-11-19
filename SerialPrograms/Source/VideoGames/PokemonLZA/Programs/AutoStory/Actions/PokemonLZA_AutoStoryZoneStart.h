/*  PokemonLZA AutoStory - Zone start
 *
 *  From: https://github.com/PokemonAutomation/
 *
  *  How 03.08 gets from its start save at Taunie's rocks to the road's
  *  spot at 100, where ZoneSearch looks for new species: a measured leg
  *  off the rocks and its read (ROCKS_NOT_LEFT), then map legs (MapLeg).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneStart_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneStart_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "PokemonLZA_AutoStoryZone1Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The search spot as the objective marker's distance: the plaza by the green advertising column.
constexpr int ZONE_SPOT_DISTANCE = 100;

//  Whether an agreed distance read is at the search spot.
bool at_zone_spot(std::optional<int> distance);


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
