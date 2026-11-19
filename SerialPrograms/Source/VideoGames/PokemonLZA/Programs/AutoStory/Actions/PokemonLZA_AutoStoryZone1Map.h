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
//  A 52-pixel leg to the 60 spot ended 7.8 pixels past it, the targets ahead.
inline constexpr double CATCH_STOP_RADIUS = 12;

//  Wild Zone 1's waypoints. The map frames put the patrol's stop 10,
//  marker read ~87, at (415, 230): about 1.2 map pixels to a marker unit
//  from the "!". The road runs along the zone's south edge (y 261 at x 380,
//  248 at 440, 228 at 500); its middle is taken 10 pixels inside it,
//  y = 245 - 0.3 (x - 410). Each waypoint is the point on that line at its
//  marker distance times 1.2 from the "!".
//  Taunie's rocks (marker 127): the pin read there before the leg off them.
//  It decides where 03.08 starts (ZoneStart::leave()).
inline constexpr MapWaypoint ZONE1_ROCKS{.name = "Taunie's rocks", .pin = {.x = 476, .y = 224}, .provisional = false};
//  The plaza by the green advertising column (marker 100, Scatterbug):
//  (432, 238) read 93 and 91 (console B, 2026-11-18), so ~10 pixels on
//  toward the rocks, and 6 north of the road's middle (~5 marker units):
//  the patrol ran too far left on the road (user, 2026-11-18).
inline constexpr MapWaypoint ZONE1_SPOT_100{.name = "the 100 spot", .pin = {.x = 442, .y = 227}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};
//  Before the far gate, Mareep's grass right of the road (marker 60), at
//  (386, 252): targets show on the right from here, none from (370, 256);
//  6 north of that, toward the grass, as the patrol ran too far left on
//  the road (user, 2026-11-18).
inline constexpr MapWaypoint ZONE1_SPOT_60{.name = "the 60 spot", .pin = {.x = 386, .y = 246}, .provisional = true, .stop_radius = CATCH_STOP_RADIUS};

//  The far gate where its "A Leave" prompt shows: the pin read between
//  its pillars (console B, 2026-11-18). The "!" at (313, 253) is past it,
//  outside the zone; runs aimed there from north of the road hit the
//  right pillar. 1 north of the (361, 253) read: both runs with no
//  prompt ended at y 255, on the left pillar (console B, 2026-11-18).
inline constexpr MapWaypoint ZONE1_FAR_GATE{.name = "the far gate", .pin = {.x = 361, .y = 252}, .provisional = false};
//  On the far gate's line, ~15 pixels east of ZONE1_GATE_APPROACH: the leg
//  from it to the approach point walks the gate's own heading, so the run
//  starts on the line facing the gate. A leg in from the north (heading
//  178) and a 90 degree turn there ended on the left pillar (console B,
//  2026-11-18).
inline constexpr MapWaypoint ZONE1_GATE_LINE{.name = "the far gate's line", .pin = {.x = 391, .y = 252}, .provisional = true};
//  On the far gate's line, ~15 pixels before it: an exact start (no stop
//  area) for the straight run to it. A run from (385, 253) or (378, 245),
//  the 60 spot's loose ends, hit a pillar (console B, 2026-11-18).
inline constexpr MapWaypoint ZONE1_GATE_APPROACH{.name = "the far gate's approach", .pin = {.x = 376, .y = 252}, .provisional = true};
//  From the rocks to the first catch spot: straight, past the rock on its
//  right (a road entry leg before it took the left side).
inline constexpr MapWaypoint ROCKS_TO_SPOT_100[] = {ZONE1_SPOT_100};

//  The waypoint of a catch spot of CATCH_SPOTS (100 or 60); none for any
//  other distance.
std::optional<MapWaypoint> catch_spot_waypoint(int spot);


//  Whether `pin`, a read of read_map_pin(), is in Wild Zone 1: inside the
//  zone's outline on the full map, traced from console B's map frames,
//  and not at the map's centre, where the map puts a player out of every
//  wild zone (see the .cpp for both).
bool pin_in_zone_1(MapPoint pin);

//  What reads the player's pin on the full map (read_pin_on_map()); none
//  when it does not show.
using PinReader = std::function<std::optional<MapPoint>()>;

//  Whether the player is out of Wild Zone 1, by the map pin: `pin` read
//  twice, out of the zone (pin_in_zone_1()) on both. Fails as
//  "in_wild_zone" on a read in the zone and as "zone_unread" on a read
//  with no pin or with no reader. Logs the verdict with the reads. 03.08b
//  saved in the zone once (console B, 2026-11-18): its menu save waits for
//  this.
Pokemon::Outcome check_out_of_zone_1(PlzaBackend& backend, const PinReader& pin);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
