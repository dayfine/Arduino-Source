/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The console-free plan of 03.08's walks along Wild Zone 1's road: the
 *  catch spots the patrol goes between.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H
#define PokemonAutomation_PokemonLZA_AutoStoryZone1Road_H

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

//  How far short of a road stop still counts as back on the road there:
//  the walk back to 60 from nearer the gate stopped at 58 once (console B,
//  2026-11-14), and the gate walk from there is steered by the gate.
inline constexpr int ROAD_STOP_SLACK = 3;

//  `visits` stops going back and forth over `spots`: {100, 60, 100, ...}.
std::vector<int> alternating_stops(std::span<const int> spots, size_t visits);


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
