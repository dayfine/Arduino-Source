/*  PokemonLZA AutoStory - Zone start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <cstdlib>
#include "PokemonLZA_AutoStoryZoneStart.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  How far off the spot a read still counts as there: the search's legs
//  are a few steps either way along the road.
constexpr int ZONE_SPOT_SLACK = 10;

} // namespace


bool at_zone_spot(std::optional<int> distance){
    return distance && std::abs(*distance - ZONE_SPOT_DISTANCE) <= ZONE_SPOT_SLACK;
}

ZoneStartMove ZoneStart::leave(std::optional<MapPoint> start_pin, std::optional<int> distance){
    if (!start_pin){
        return next(distance);
    }
    const MapPoint start = m_route.start.pin;
    const bool at_start = std::hypot(start_pin->x - start.x, start_pin->y - start.y) <= m_route.start_radius;
    m_last = at_start ? ZoneStartMove::WALK_TO_SPOT : ZoneStartMove::ROCKS_NOT_LEFT;
    return *m_last;
}

ZoneStartMove ZoneStart::next(std::optional<int> distance){
    if (!m_last){
        const bool in_range = distance && *distance >= m_route.leg_read_min && *distance <= m_route.leg_read_max;
        m_last = in_range ? ZoneStartMove::WALK_TO_SPOT : ZoneStartMove::ROCKS_NOT_LEFT;
    }else if (*m_last == ZoneStartMove::WALK_TO_SPOT){
        //  No read is trusted to the timed walk: it lands at 101 to 105 every
        //  run, where the digits over the green column read "1", "41" and
        //  "5" (console B, 2026-11-16 16:14, 101 on screen).
        m_last = !distance || at_zone_spot(distance) ? ZoneStartMove::SEARCH : ZoneStartMove::SPOT_NOT_REACHED;
    }
    return *m_last;
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
