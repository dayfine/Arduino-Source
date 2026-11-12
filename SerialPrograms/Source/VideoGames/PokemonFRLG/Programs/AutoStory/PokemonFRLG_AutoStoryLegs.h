/*  AutoStory Legs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  LEGS steps: a fixed list of legs, as the walker's route steps.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLegs_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLegs_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

//  The variant of LEGS walked on foot a tap at a time, each tap checked to
//  have moved the player a tile (RouteStep::tapped): the Rocket Hideout's
//  arrow floors, where a hold a tile off slides the player elsewhere.
inline const std::string TAPS_VARIANT = "taps";

//  `legs` from `start` as route steps for the walker, each checked against
//  the map: nothing if a step is blocked or leaves the map. A leg ends "to
//  wall" if the next step its way is blocked.
std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<Pokemon::StoryLeg>& legs
);

//  The one-tile walk a press of `direction` from `from` makes, if it takes
//  a warp: an exit mat stood on, a door ahead going north, or a warp tile
//  stepped onto. It ends where the warp leaves the player, facing the way
//  the game turns them (arrival_facing(), as the planner takes a warp). Its
//  fade and the scene after it must then be confirmed
//  (OverworldWalkSteps::wait_out_warp()), not taken as walked.
std::optional<RouteStep> warp_press(const std::vector<MapData>& maps, const MapPosition& from, Direction direction);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
