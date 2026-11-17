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

//  The walk's pace on the map. CONSOLE (console B, 2026-11-16 and
//  2026-11-17, lab-B logs): 35 legs, each "map leg ... from (x, y) ...
//  hold N ms" and the next "map pin" read, one stuck leg left out.
//  Median 6.9 pixels a second (quartiles 6.6 to 7.0, 5.4 to 7.3
//  overall); the 12 legs of 5 s or more 7.0 (6.7 to 7.3); a fit of
//  distance on hold 7.05 a second, less 0.9 pixels. At 6.7 every long
//  leg overshot.
constexpr double WALK_PIXELS_PER_SECOND = 7.0;

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


bool MapLeg::in_stop_area(MapPoint pin, double distance) const{
    if (distance > m_stop_radius){
        return false;
    }
    if (!m_walked_from){
        return true;
    }
    //  Past the target on the walk's way: the miss projects forward onto
    //  the walk's direction.
    const double way_x = m_target.x - m_walked_from->x;
    const double way_y = m_target.y - m_walked_from->y;
    return (pin.x - m_target.x) * way_x + (pin.y - m_target.y) * way_y >= 0;
}

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
    if (distance <= TOLERANCE || in_stop_area(*pin, distance)){
        return {.step = MapLegStep::ARRIVED};
    }
    if (m_walks > CORRECTIONS){
        return {.step = MapLegStep::OFF_TARGET};
    }
    m_walked_from = *pin;
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
