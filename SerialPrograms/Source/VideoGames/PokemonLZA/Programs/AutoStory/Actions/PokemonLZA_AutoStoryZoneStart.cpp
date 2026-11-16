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
        m_last = at_zone_spot(distance) ? ZoneStartMove::SEARCH : ZoneStartMove::LEAVE_WEST;
    }else if (*m_last == ZoneStartMove::LEAVE_WEST){
        if (distance){
            m_last = ZoneStartMove::WALK_TO_SPOT;
        }else if (m_west_steps >= m_max_west_steps){
            m_last = ZoneStartMove::ROCKS_NOT_LEFT;
        }
    }else if (*m_last == ZoneStartMove::WALK_TO_SPOT){
        m_last = at_zone_spot(distance) ? ZoneStartMove::SEARCH : ZoneStartMove::SPOT_NOT_REACHED;
    }
    if (*m_last == ZoneStartMove::LEAVE_WEST){
        m_west_steps++;
    }
    return *m_last;
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
