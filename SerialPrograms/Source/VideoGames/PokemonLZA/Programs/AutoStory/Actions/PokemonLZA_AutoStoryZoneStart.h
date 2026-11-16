/*  PokemonLZA AutoStory - Zone start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How 03.08 gets from its start save at Taunie's rocks to the road's
 *  spot at 100, where ZoneSearch looks for new species: a measured leg
 *  off the rocks, then the marker walk (the legs measured by hand on
 *  console B, 2026-11-16). Run 9 (console B, 2026-11-16) searched at the
 *  rocks: two full turns with no name plate. Run 10 stepped west by the
 *  minimap with a marker read after each step and never read the marker:
 *  the camera faced away from it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZoneStart_H
#define PokemonAutomation_PokemonLZA_AutoStoryZoneStart_H

#include <optional>
#include "Common/Cpp/Time.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The search spot, as the objective marker's distance: the plaza on the
//  road by the green advertising column (the user's pick, 2026-11-13),
//  where runs 5 and 6 threw (console B, 2026-11-16).
constexpr int ZONE_SPOT_DISTANCE = 100;

//  Whether an agreed distance read is at the search spot.
bool at_zone_spot(std::optional<int> distance);


//  A start's way to the search spot: L puts the camera behind the player,
//  a leg off the start with the left stick, camera-relative, an agreed
//  marker read that must fall in the leg's range, then the marker walk
//  toward the objective until a read at or below `walk_stop`.
struct ZoneStartRoute{
    double leg_x;
    double leg_y;
    Milliseconds leg_hold;
    int leg_read_min;
    int leg_read_max;
    int walk_stop;
    Milliseconds walk_limit;
};

//  From the 03.08 save at Taunie's rocks, measured by hand on console B
//  (2026-11-16): the player faces the rocks, the marker centred above
//  them at 127. Left 900 ms sidesteps clear of the rocks onto the grass
//  strip, still 127; then forward the marker read 113 after 2.5 s and 102
//  after 4.8 s, at the plaza (about 5.6 a second), with every read clean.
constexpr ZoneStartRoute TAUNIE_ROCKS_ROUTE{
    .leg_x = -1,
    .leg_y = 0,
    .leg_hold = Milliseconds(900),
    .leg_read_min = 120,
    .leg_read_max = 135,
    .walk_stop = 103,
    .walk_limit = Milliseconds(8000),
};


//  What the way to the search spot does next.
enum class ZoneStartMove{
    WALK_TO_SPOT,       //  the marker walk to the route's walk_stop
    SEARCH,             //  at the spot: the search (ZoneSearch)
    ROCKS_NOT_LEFT,     //  stop: the read after the leg is missing or out of its range
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
