/*  PokemonLZA AutoStory - Wild Zone 1 map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's waypoints in Wild Zone 1 on the full map (read_map_pin()):
 *  where its map legs (MapLeg) end.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Map_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Map_H

#include <optional>
#include <string_view>
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMapPinReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Where a leg ends on the full map.
struct MapWaypoint{
    std::string_view name = {};
    MapPoint pin = {};
    //  Derived from two map frames and the "!" (the far gate) at about
    //  (313, 253), not measured where the player stood: a console run's
    //  "[PLZA] map pin" lines calibrate it.
    bool provisional = true;
};

//  Wild Zone 1's waypoints. The map frames of console B (2026-11-16) put
//  the patrol's stop 10, marker read ~87, at (415, 230): about 1.2 map
//  pixels to a marker unit from the "!". The road runs along the zone's
//  south edge (y 261 at x 380, 248 at 440, 228 at 500); its middle is
//  taken 10 pixels inside it, y = 245 - 0.3 (x - 410). Each waypoint is
//  the point on that line at its marker distance times 1.2 from the "!".
//  Taunie's rocks (marker 127): only logged against the pin read there.
inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 464, .y = 229}, .provisional = true};
//  The road off the rocks (marker ~120).
inline constexpr MapWaypoint ZONE1_ROAD_ENTRY{.name = "the road entry", .pin = {.x = 456, .y = 231}, .provisional = true};
//  The plaza by the green advertising column (marker 100, Scatterbug).
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 432, .y = 238}, .provisional = true};
//  Before the far gate, Mareep's grass right of the road (marker 60).
//  Moved on from (386, 252) toward the Mareep seen from there (console
//  B, 2026-11-16 18:43): the leg ended at (384, 253) facing 245, the
//  target's brackets at x 0.69 (11 degrees right, the view taken as 60
//  wide), and 4 steps of 0.5 s toward it brought its plate from y 0.31 to
//  0.45. 2 s at the leg's 7.0 map pixels a second (49.8 pixels in its
//  7104 ms hold) is 14 pixels on heading 256 from (384, 253): (-13.6,
//  +3.4), so (370, 256), on the road's middle line (y 257 at x 370).
inline constexpr MapWaypoint ZONE1_SPOT_60{.name = "the 60 spot", .pin = {.x = 370, .y = 256}, .provisional = true};

//  From the rocks to the first catch spot.
inline constexpr MapWaypoint ROCKS_TO_SPOT_100[] = {ZONE1_ROAD_ENTRY, ZONE1_SPOT_100};

//  The waypoint of a catch spot of CATCH_SPOTS (100 or 60); none for any
//  other distance.
std::optional<MapWaypoint> catch_spot_waypoint(int spot);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
