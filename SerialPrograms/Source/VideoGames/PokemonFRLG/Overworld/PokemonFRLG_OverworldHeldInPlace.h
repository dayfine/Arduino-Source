/*  Overworld Held In Place
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a hold that something stops at once leaves the player, from the
 *  tiles it may have been held from: below or above a sign that speaks at
 *  every hold, or before a person who stops it without a step.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldHeldInPlace_H
#define PokemonAutomation_PokemonFRLG_OverworldHeldInPlace_H

#include <vector>
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Of the tiles a hold of `step`, a leg to a wall north or south, may
//  start from, `starts`, where the hold stops below or above a sign, which
//  it then reads: where the player stands when every hold of the leg is
//  interrupted at once by a message.
std::vector<StopCandidate> before_signs(const std::vector<MapData>& maps, const std::vector<MapPosition>& starts, const RouteStep& step);

//  Where a hold of `hold` that moved nothing, not even a step to a wall,
//  may have been held from: of `tiles`, and those up to `across` beside
//  each of them square to its way, the ones the player may stand on with a
//  person the maps place on the next tile its way. A person stops a hold
//  without a step, as a wall does.
std::vector<StopCandidate> before_people(
    const std::vector<MapData>& maps, const std::vector<MapPosition>& tiles, const RouteStep& hold, int across
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
