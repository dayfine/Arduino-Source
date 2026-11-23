/*  AutoStory Legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLegs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLegs_H

#include <optional>
#include <string>
#include <vector>
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


inline const std::string TAPS_VARIANT = "taps";

std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<StoryLeg>& legs
);

std::optional<RouteStep> warp_press(const std::vector<MapData>& maps, const MapPosition& from, Direction direction);

bool takes_mat_warp(const std::vector<MapData>& maps, const MapPosition& from, Direction direction);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
