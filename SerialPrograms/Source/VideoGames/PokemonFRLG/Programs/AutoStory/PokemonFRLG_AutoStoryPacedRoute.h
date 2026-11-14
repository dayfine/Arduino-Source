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
#include <set>
#include <string>
#include <string_view>
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
//  With `run_flags` (a run's, default none): without the Rockets once
//  SILPH_CLEARED_FLAG is among them.
const std::vector<MapData>& story_maps(const std::set<std::string>& run_flags = {});
//  The run flag Giovanni's step on 11F sets: the story's maps from then on
//  have no Rockets in Silph Co. or Saffron City (15.01 to 16.xx before it
//  have them).
inline constexpr std::string_view SILPH_CLEARED_FLAG = "silph_cleared";
//  A run flag that says a trainer is beaten: this prefix, then the
//  trainer's trainer_id() ("trainer_beaten:Route21_South/swimmer_m_water@10,29").
//  A checkpoint's start_flags list those its save has beaten; a step that
//  fights one sets it in flags_after. The walker doesn't watch for them.
inline constexpr std::string_view TRAINER_BEATEN_FLAG = "trainer_beaten:";
//  The trainers `run_flags` says are beaten (TRAINER_BEATEN_FLAG), by
//  trainer_id().
std::set<std::string> beaten_trainers(const std::set<std::string>& run_flags);
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
