/*  PokemonLZA AutoStory - Wild Zone 1 map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <span>
#include "PokemonLZA_AutoStoryZone1Map.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::optional<MapWaypoint> catch_spot_waypoint(int spot){
    switch (spot){
    case 100:
        return ZONE1_SPOT_100;
    case 60:
        return ZONE1_SPOT_60;
    default:
        return std::nullopt;
    }
}

namespace{

//  Wild Zone 1's green outline on the full map, clockwise from its north
//  west corner: traced on the map frames of console B (2026-11-16,
//  zone1-map-after-try6 and zone1-map-at-north-fence), the bottom edge
//  by the outline's lowest green in each column. Every pin read in the
//  zone on console B (2026-11-16, 2026-11-17: the rocks (476, 224), the
//  road entry, the 100 and 60 spots, the north fence (359, 227) and the
//  catch west of the 60 spot (365, 257)) is inside it.
constexpr MapPoint ZONE1_OUTLINE[] = {
    {.x = 281, .y = 199}, {.x = 506, .y = 199}, {.x = 515, .y = 218}, {.x = 510, .y = 226},
    {.x = 500, .y = 230}, {.x = 450, .y = 248}, {.x = 400, .y = 260}, {.x = 370, .y = 265},
    {.x = 359, .y = 264}, {.x = 357, .y = 248}, {.x = 281, .y = 249},
};

//  Where the full map puts the player when the player is out of every
//  wild zone: the map opens on the player, at its centre (400, 225),
//  instead of on the zone. On the street outside the far gate (03.08b's
//  menu save, the minimap's arrow by the gate's "!") the pin read
//  (401, 225) and (401, 223) (console B, 2026-11-17 01:06), the middle of
//  the zone's outline; no pin in the zone read within 15 of it. From these
//  two reads only: no map frame of the street yet.
constexpr MapPoint MAP_CENTRE{.x = 400, .y = 225};
constexpr double MAP_CENTRE_RADIUS = 6;

//  Whether `point` is inside the polygon `outline` (even-odd rule).
bool inside(std::span<const MapPoint> outline, MapPoint point){
    bool in = false;
    size_t previous = outline.size() - 1;
    for (size_t index = 0; index < outline.size(); previous = index++){
        const MapPoint& a = outline[index];
        const MapPoint& b = outline[previous];
        if ((a.y > point.y) == (b.y > point.y)){
            continue;
        }
        const double crossing_x = a.x + (point.y - a.y) * (b.x - a.x) / (b.y - a.y);
        if (point.x < crossing_x){
            in = !in;
        }
    }
    return in;
}

} // namespace

bool pin_in_zone_1(MapPoint pin){
    if (std::hypot(pin.x - MAP_CENTRE.x, pin.y - MAP_CENTRE.y) <= MAP_CENTRE_RADIUS){
        return false;
    }
    return inside(ZONE1_OUTLINE, pin);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
