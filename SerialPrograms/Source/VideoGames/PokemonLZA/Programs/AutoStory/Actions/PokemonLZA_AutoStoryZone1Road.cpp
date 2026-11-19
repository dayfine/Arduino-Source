/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <cstdlib>
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::vector<int> alternating_stops(std::span<const int> spots, size_t visits){
    std::vector<int> stops;
    if (spots.empty()){
        return stops;
    }
    for (size_t i = 0; i < visits; i++){
        stops.push_back(spots[i % spots.size()]);
    }
    return stops;
}


namespace{

//  The search spot as the objective marker's distance: the plaza by the green advertising column.
constexpr int ZONE_SPOT_DISTANCE = 100;
//  The search's legs are a few steps either way along the road.
constexpr int ZONE_SPOT_SLACK = 10;

//  Whether an agreed distance read is at the search spot.
bool at_zone_spot(std::optional<int> distance){
    return distance && std::abs(*distance - ZONE_SPOT_DISTANCE) <= ZONE_SPOT_SLACK;
}

} // namespace

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
        //  No read passes: the walk lands at 101 to 105, where the digits over
        //  the green column often misread.
        m_last = !distance || at_zone_spot(distance) ? ZoneStartMove::SEARCH : ZoneStartMove::SPOT_NOT_REACHED;
    }
    return *m_last;
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
