/*  PokemonLZA AutoStory - Wild Zone 1 map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <span>
#include <string>
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
//  west corner, traced from map frames; every pin read in the zone so far
//  is inside it.
constexpr MapPoint ZONE1_OUTLINE[] = {
    {.x = 281, .y = 199}, {.x = 506, .y = 199}, {.x = 515, .y = 218}, {.x = 510, .y = 226},
    {.x = 500, .y = 230}, {.x = 450, .y = 248}, {.x = 400, .y = 260}, {.x = 370, .y = 265},
    {.x = 359, .y = 264}, {.x = 357, .y = 248}, {.x = 281, .y = 249},
};

//  Out of every wild zone the map opens on the player at its centre: on the
//  street outside the far gate the pin read (401, 225) and (401, 223); no
//  pin in the zone read within 15 of it.
constexpr MapPoint MAP_CENTRE{.x = 400, .y = 225};
constexpr double MAP_CENTRE_RADIUS = 6;

constexpr size_t OUT_OF_ZONE_PIN_READS = 2;

//  Even-odd rule.
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

std::optional<Pokemon::StoryFailure> check_out_of_zone_1(PlzaBackend& backend, MoveOnlyFunction<std::optional<MapPoint>()> pin){
    std::string reads;
    for (size_t read = 1; read <= OUT_OF_ZONE_PIN_READS; read++){
        const std::optional<MapPoint> at = pin ? pin() : std::nullopt;
        if (!at){
            const std::string detail = std::format(
                "no map pin on read {} of {}{}", read, OUT_OF_ZONE_PIN_READS, reads.empty() ? "" : std::format(" (before: {})", reads)
            );
            backend.log(std::format("[PLZA] out of Wild Zone 1 not known: {}", detail));
            return Pokemon::StoryFailure{.code = "zone_unread", .detail = std::format("out of Wild Zone 1 not known: {}", detail)};
        }
        reads += std::format("{}({:.0f}, {:.0f})", reads.empty() ? "" : ", ", at->x, at->y);
        if (pin_in_zone_1(*at)){
            backend.log(std::format("[PLZA] still in Wild Zone 1: the map pin reads {}", reads));
            return Pokemon::StoryFailure{.code = "in_wild_zone", .detail = std::format("still in Wild Zone 1: the map pin reads {}", reads)};
        }
    }
    backend.log(std::format("[PLZA] out of Wild Zone 1: the map pin reads {}", reads));
    return std::nullopt;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
