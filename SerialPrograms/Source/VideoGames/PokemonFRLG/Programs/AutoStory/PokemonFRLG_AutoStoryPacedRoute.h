/*  AutoStory Paced Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a step that paces (a GRIND or capture) leaves the player, and the
 *  route on from there.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPacedRoute_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPacedRoute_H

#include <optional>
#include <vector>
#include "Pokemon/Navigation/Pokemon_GatedTypes.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a GRIND or capture `step` that paces (pace = ...) leaves the
//  player who started it on `patch`: the patch's tile or the one its pace
//  leads to, as the last battle came, facing unknown. `patch` for any
//  other step, or a `patch` not on one tile.
Pokemon::Pose paced_pose(const Pokemon::Pose& patch, const StoryStep& step);

//  The maps the story plans on: every map to Route 4's Pokemon Center, with
//  the objects the game hides after the Hall of Fame removed, as the lab's
//  frlg_route does, and those a new game hides until their scene (the
//  S.S. Anne's rival on the Captain's door).
const std::vector<MapData>& story_maps();
//  default_route_costs() without Surf, which the story hasn't got yet:
//  routes stay on land, even where water is the shorter way (Vermilion
//  City's harbor).
const RouteCosts& story_route_costs();

//  A ROUTE's legs from `pose` to `goal`, on story_route_costs(): from
//  one tile, as planned; from a set of tiles (paced_pose()), first the
//  holds to walls that bring them all to one tile (holds_to_one_tile()),
//  then the route from there.
//  Nothing if either can't be planned.
std::optional<std::vector<RouteStep>> plan_story_route(
    const std::vector<MapData>& maps, const Pokemon::Pose& pose,
    std::optional<Direction> facing, const RouteGoal& goal,
    const Pokemon::GateState& gates = Pokemon::GateState(), const Pokemon::GateRules& rules = Pokemon::GateRules()
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
