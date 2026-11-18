/*  Overworld Press
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one D-pad press does to a standing player, as the route planner's
 *  own collision and warp rules predict it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldPress_H
#define PokemonAutomation_PokemonFRLG_OverworldPress_H

#include <cstdint>
#include <optional>
#include <vector>
#include "PokemonFRLG_OverworldMap.h"
#include "PokemonFRLG_OverworldRouteSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A stationary prediction in the map's runtime-local tile/elevation frame.
struct OverworldPose{
    MapPosition position;
    uint8_t elevation = ELEVATION_GROUND;
    bool surfing = false;
    Direction facing = Direction::NORTH;
};
//  One transition from the production planner's own collision/warp implementation.
struct OverworldPress{
    OverworldPose pose;
    bool warped = false;
};
//  A model prediction only; nullopt means no known move, never confirmed localization.
std::optional<OverworldPress> predict_overworld_press(
    const std::vector<MapData>& maps, const OverworldPose& from, Direction direction
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
