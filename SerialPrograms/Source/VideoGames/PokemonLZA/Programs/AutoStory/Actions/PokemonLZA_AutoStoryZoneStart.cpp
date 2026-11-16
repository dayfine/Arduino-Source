/*  PokemonLZA AutoStory - Zone start
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

ZoneStartMove ZoneStart::next(std::optional<int> distance){
    if (!m_last){
        const bool in_range = distance && *distance >= m_route.leg_read_min && *distance <= m_route.leg_read_max;
        m_last = in_range ? ZoneStartMove::WALK_TO_SPOT : ZoneStartMove::ROCKS_NOT_LEFT;
    }else if (*m_last == ZoneStartMove::WALK_TO_SPOT){
        m_last = at_zone_spot(distance) ? ZoneStartMove::SEARCH : ZoneStartMove::SPOT_NOT_REACHED;
    }
    return *m_last;
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
