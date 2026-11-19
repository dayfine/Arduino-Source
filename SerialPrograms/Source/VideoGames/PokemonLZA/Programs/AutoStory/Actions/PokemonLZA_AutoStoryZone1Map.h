/*  PokemonLZA AutoStory - Wild Zone 1 map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  03.08's waypoints in Wild Zone 1 on the full map (read_map_pin()):
 *  where its map legs (MapLeg) end, and whether the player is out of the
 *  zone.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Map_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Map_H

#include <functional>
#include <optional>
#include <string_view>
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMapPinReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Where a leg ends on the full map.
struct MapWaypoint{
    std::string_view name = {};
    MapPoint pin = {};
    //  Derived from map frames and the "!" (the far gate) at about (313, 253),
    //  not measured where the player stood.
    bool provisional = true;
    //  A read up to this many map pixels from `pin` counts as there when it is
    //  past `pin` on the leg's way, or the leg's first read. 0: only the leg's
    //  tolerance.
    double stop_radius = 0;
};

//  ~10 marker units: a 52-pixel leg to the 60 spot ended 7.8 pixels past it.
inline constexpr double CATCH_STOP_RADIUS = 12;

//  About 1.2 map pixels to a marker unit from the "!"; the road's middle is
//  y = 245 - 0.3 (x - 410). Each waypoint is the point on that line at its
//  marker distance times 1.2 from the "!".
//  Taunie's rocks (marker 127): where 03.08 starts.
inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 476, .y = 224}, .provisional = false};
//  The plaza by the green advertising column (marker 100): (432, 238) read
//  93 and 91, so ~10 pixels on toward the rocks, and 6 north of the road's
//  middle: the patrol ran too far left on the road.
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 442, .y = 227}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};
//  Mareep's grass right of the road (marker 60): targets show on the right
//  from (386, 252), none from (370, 256); 6 north of that, toward the grass.
inline constexpr MapWaypoint ZONE1_SPOT_60{.name = "the 60 spot", .pin = {.x = 386, .y = 246}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};

//  The pin between the gate's pillars, where "A Leave" shows; 1 north of
//  the (361, 253) read, as runs with no prompt ended at y 255 on the left
//  pillar.
inline constexpr MapWaypoint ZONE1_FAR_GATE{.name = "the far gate", .pin = {.x = 361, .y = 252}, .provisional = false};
//  ~15 pixels east of ZONE1_GATE_APPROACH, so the leg from it walks the
//  gate's own heading; a leg in from the north and a 90 degree turn there
//  ended on the left pillar.
inline constexpr MapWaypoint ZONE1_GATE_LINE{.name = "the far gate's line", .pin = {.x = 391, .y = 252}, .provisional = true};
//  ~15 pixels before the gate, an exact start (no stop area): runs from the
//  60 spot's loose ends hit a pillar.
inline constexpr MapWaypoint ZONE1_GATE_APPROACH{.name = "the far gate's approach", .pin = {.x = 376, .y = 252}, .provisional = true};
//  Straight, past the rock on its right.
inline constexpr MapWaypoint ROCKS_TO_SPOT_100[] = {ZONE1_SPOT_100};

//  None for distances other than CATCH_SPOTS.
std::optional<MapWaypoint> catch_spot_waypoint(int spot);


//  Inside the zone's outline and not at the map's centre, where the map
//  puts a player out of every wild zone.
bool pin_in_zone_1(MapPoint pin);

//  None when the pin does not show.
using PinReader = std::function<std::optional<MapPoint>()>;

//  Reads `pin` twice; out only when both are. Fails as "in_wild_zone" on a
//  read in the zone and as "zone_unread" on a read with no pin or reader.
Pokemon::Outcome check_out_of_zone_1(PlzaBackend& backend, const PinReader& pin);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
