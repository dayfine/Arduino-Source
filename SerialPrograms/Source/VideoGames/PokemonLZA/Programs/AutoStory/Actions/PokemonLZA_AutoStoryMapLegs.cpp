/*  PokemonLZA AutoStory - Map legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <numbers>
#include "PokemonLZA_AutoStoryMapLegs.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The walk's pace on the map: 5.6 marker units a second from the rocks
//  to the plaza (console B, 2026-11-16) at ~1.2 map pixels a unit. NEEDS
//  CONSOLE: calibrate from a leg's hold and the pins read before and
//  after it.
constexpr double WALK_PIXELS_PER_SECOND = 6.7;

//  How far from a waypoint, in map pixels, counts as at it: ~5 marker
//  units.
constexpr double TOLERANCE = 6;
//  The short correction legs after a leg that ended off its waypoint.
constexpr size_t CORRECTIONS = 2;

//  The minimap's heading from `from` to `to`: 0 up, 90 right.
double bearing(MapPoint from, MapPoint to){
    const double degrees = std::atan2(to.x - from.x, from.y - to.y) * 180 / std::numbers::pi;
    return degrees < 0 ? degrees + 360 : degrees;
}

} // namespace


MapLegMove MapLeg::next(std::optional<MapPoint> pin){
    if (!pin){
        if (m_missed){
            return {.step = MapLegStep::PIN_NOT_READ};
        }
        m_missed = true;
        return {.step = MapLegStep::READ_AGAIN};
    }
    m_missed = false;
    const double distance = std::hypot(m_target.x - pin->x, m_target.y - pin->y);
    if (distance <= TOLERANCE){
        return {.step = MapLegStep::ARRIVED};
    }
    if (m_walks > CORRECTIONS){
        return {.step = MapLegStep::OFF_TARGET};
    }
    return {
        .step = MapLegStep::WALK,
        .heading = bearing(*pin, m_target),
        .hold = Milliseconds((int64_t)(distance / WALK_PIXELS_PER_SECOND * 1000)),
        .correction = m_walks++,
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
