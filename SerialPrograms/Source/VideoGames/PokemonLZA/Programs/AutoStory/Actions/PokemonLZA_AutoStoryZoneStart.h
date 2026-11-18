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


//  The search spot, as the objective marker's distance: the plaza on the
//  road by the green advertising column (the user's pick).
constexpr int ZONE_SPOT_DISTANCE = 100;

//  Whether an agreed distance read is at the search spot.
bool at_zone_spot(std::optional<int> distance);


//  A start's way to the search spot: L puts the camera behind the player,
//  a leg off the start with the left stick, camera-relative, an agreed
//  marker read that must fall in the leg's range, then the marker walk
//  toward the objective for `walk_hold`, then one read.
//  The pin read at the start, within `start_radius` map pixels of
//  `start`, says the player stands there (ZoneStart::leave()).
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

//  From the 03.08 save at Taunie's rocks, measured by hand: the player
//  faces the rocks, the marker at 127. Left 900 ms sidesteps clear of
//  them onto the grass strip, still 127; then forward reads 113 after
//  2.5 s and 102 after 4.8 s, at the plaza. The pin at the rocks reads
//  (476, 224); the leg off them moves it 4 to 5 pixels.
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
    WALK_TO_SPOT,       //  the timed walk toward the objective (walk_hold)
    SEARCH,             //  at the spot: the search (ZoneSearch)
    ROCKS_NOT_LEFT,     //  stop: not at the start by the pin, or, the pin unread, the read after the leg is missing or out of its range
    SPOT_NOT_REACHED,   //  stop: the marker walk did not end at the spot
};

//  A ZoneStartRoute read by read, from the read after its leg: in the
//  leg's range, the marker walk; then at the spot, the search. A stop
//  stays a stop.
class ZoneStart{
public:
    explicit ZoneStart(const ZoneStartRoute& route)
        : m_route(route)
    {}

    //  The move off the start: `start_pin`, the pin read before the leg,
    //  decides (the digits may not read off the rocks, as in French);
    //  `distance`, the agreed read after the leg, only when the pin did not
    //  read (none: no read).
    ZoneStartMove leave(std::optional<MapPoint> start_pin, std::optional<int> distance);

    //  The move after `distance`, the agreed read (none: no read) after
    //  the leg, then at the end of the marker walk.
    ZoneStartMove next(std::optional<int> distance);

private:
    const ZoneStartRoute m_route;
    std::optional<ZoneStartMove> m_last;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
