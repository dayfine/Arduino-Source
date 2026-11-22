/*  AutoStory Paced Route
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a step that paces (a grind or capture) leaves the player, and the
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
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/Navigation/Grid/Planning/Pokemon_GatedTypes.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_Grid.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where a grind or capture `step` that paces leaves the player who started
//  it on `patch`: the patch's tile or the one its pace leads to, facing unknown.
std::optional<Pokemon::GridPose> paced_pose(const std::optional<Pokemon::GridPose>& patch, const StoryStep& step);

const std::vector<MapData>& story_maps();
const std::vector<MapData>& run_story_maps(const std::set<std::string>& run_flags);

inline constexpr std::string_view SILPH_CLEARED_FLAG = "silph_cleared";
inline constexpr std::string_view TRAINER_BEATEN_FLAG = "trainer_beaten:";

std::set<std::string> beaten_trainers(const std::set<std::string>& run_flags);

const RouteCosts& story_route_costs();

std::optional<std::vector<RouteStep>> plan_story_route(
    const std::vector<MapData>& maps, const Pokemon::GridPose& pose,
    std::optional<Direction> facing, const RouteGoal& goal,
    const Pokemon::GateState& gates = Pokemon::GateState(),
    const Pokemon::GateRules& rules = Pokemon::GateRules()
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
