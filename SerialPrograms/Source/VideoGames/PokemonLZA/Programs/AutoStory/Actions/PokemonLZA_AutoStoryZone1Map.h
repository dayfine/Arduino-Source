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
    //  A catch spot is a search area: a read up to this many map pixels
    //  from `pin` counts as there when it is past `pin` on the leg's way,
    //  or the leg's first read (MapLeg). 0: only the leg's tolerance.
    double stop_radius = 0;
};

//  A catch spot's stop area (MapWaypoint::stop_radius): ~10 marker units.
//  A 52-pixel leg to the 60 spot ended 7.8 pixels past it on its way
//  (console B, 2026-11-17), with the targets ahead.
inline constexpr double CATCH_STOP_RADIUS = 12;

//  Wild Zone 1's waypoints. The map frames of console B (2026-11-16) put
//  the patrol's stop 10, marker read ~87, at (415, 230): about 1.2 map
//  pixels to a marker unit from the "!". The road runs along the zone's
//  south edge (y 261 at x 380, 248 at 440, 228 at 500); its middle is
//  taken 10 pixels inside it, y = 245 - 0.3 (x - 410). Each waypoint is
//  the point on that line at its marker distance times 1.2 from the "!".
//  Taunie's rocks (marker 127): only logged against the pin read there.
//  The road entry is exact (no stop area): the leg after it starts there.
inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 464, .y = 229}, .provisional = true};
//  The road off the rocks (marker ~120).
inline constexpr MapWaypoint ZONE1_ROAD_ENTRY{.name = "the road entry", .pin = {.x = 456, .y = 231}, .provisional = true};
//  The plaza by the green advertising column (marker 100, Scatterbug).
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 432, .y = 238}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};
//  Before the far gate, Mareep's grass right of the road (marker 60).
//  Back at (386, 252): moved west to (370, 256) toward a Mareep seen
//  from here, run 3 on console B (2026-11-16 19:20 to 19:23) saw no
//  brackets there twice and only a caught species once; from (386, 252)
//  targets showed on the right (18:43). It is also where 03.08b's walk
//  to the far gate (heading 265) passed from, at 17:34 and 18:46.
inline constexpr MapWaypoint ZONE1_SPOT_60{.name = "the 60 spot", .pin = {.x = 386, .y = 252}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};

//  From the rocks to the first catch spot.
inline constexpr MapWaypoint ROCKS_TO_SPOT_100[] = {ZONE1_ROAD_ENTRY, ZONE1_SPOT_100};

//  The waypoint of a catch spot of CATCH_SPOTS (100 or 60); none for any
//  other distance.
std::optional<MapWaypoint> catch_spot_waypoint(int spot);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
