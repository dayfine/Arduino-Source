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
//  The variant of LEGS tapped blind (RouteStep::blind_taps): the spinner
//  mazes, whose slides the tap check misread as no move.
inline const std::string BLIND_TAPS_VARIANT = "blind_taps";

//  `legs` from `start` as route steps for the walker, each checked against
//  the map: nothing if a step is blocked or leaves the map. A leg ends "to
//  wall" if the next step its way is blocked.
std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<Pokemon::StoryLeg>& legs
);

//  Marks each of `route`'s steps tapped (TAPS_VARIANT).
void tap_legs(std::vector<RouteStep>& route);
//  Marks each of `route`'s steps tapped blind (BLIND_TAPS_VARIANT), from
//  `facing`: a turn tap first where the player faces another way.
void blind_tap_legs(Direction facing, std::vector<RouteStep>& route);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
