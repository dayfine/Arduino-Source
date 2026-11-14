/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free plan of 03.08's walks along Wild Zone 1's road: the
 *  catch spots the patrol goes between, the road stops back to the far
 *  gate, and where the checkpoint starts in the zone.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H

#include <chrono>
#include <optional>
#include <span>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Wild Zone 1's two catch spots on its road, as distances from the
//  objective marker (the zone's far gate), the user's pick (2026-11-13):
//  100, the plaza by the green advertising column (Scatterbug on the
//  road), and 60, just before the far gate (Mareep in the grass right of
//  the road and on it). The patrol goes 100, 60, 100, 60, ...: walking to
//  100 and back makes the Pokemon at 60 come back when none showed there.
inline constexpr int CATCH_SPOTS[] = {100, 60};

//  The left stick held right (facing the far gate) that takes the player
//  from where the walk to a catch spot ends to the middle of the road: the
//  walk ended on the left sidewalk or grass at both spots, by the stall at
//  60 and the advertising column at 100 (console B, 2026-11-13), and the
//  user caught from mid-road. The way back is the same time left. A guess
//  from the shots (the road some 5 m wide there), NEEDS CONSOLE.
inline constexpr std::chrono::milliseconds ROAD_MIDDLE_STEP{1000};

//  `visits` stops going back and forth over `spots`: {100, 60, 100, ...}.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);

//  The stops on the road to walk through, in order, from `distance` from
//  the far gate, before the last walk to the gate's "A Leave": the spots
//  of `spots` nearer the gate than `distance`, furthest first. At or past
//  the spot nearest the gate, that spot: back onto the road first, as a
//  walk straight at the gate from off the road ran into a building west
//  of the zone (console B, 2026-11-13, last read 49). With no distance
//  read, every spot.
std::vector<int> road_stops_to_gate(std::span<const int> spots, std::optional<int> distance);

//  How far Wild Zone 1's far gate (the objective marker) reads where 03.08
//  starts, just inside the zone's entrance gate: 127 on 03.08's start
//  frame (CatchesToZone1Start/0077, console B 2026-11-14), and about 115
//  to 130 where 03.07's tutorial leaves the player (catch_in_zone() steps
//  around Taunie's rocks at 115 and up). 03.07d walks to this distance.
inline constexpr int ZONE_1_START_DISTANCE = 127;

//  How far from ZONE_1_START_DISTANCE still counts as 03.08's start: the
//  marker reads whole units and a walk stops a step or two late.
inline constexpr int ZONE_1_START_TOLERANCE = 10;

//  Whether 03.07d's end is 03.08's start: a far-gate distance read
//  (nothing: none read) within ZONE_1_START_TOLERANCE of
//  ZONE_1_START_DISTANCE, and the minimap's player arrow in the wild
//  zone's green (`arrow_in_zone`, MinimapZoneDetector). The distance alone
//  is not enough: the street outside the entrance gate reads near it too.
bool at_zone_1_start(std::optional<int> distance, bool arrow_in_zone);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
