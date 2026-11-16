/*  Overworld Map
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's walking rules on a map (a Pokemon::MapData): per tile collision,
 *  elevation and the behaviors that matter for walking (water, one-way
 *  edges), plus its warps and the objects standing on it. The map data is
 *  generated from the pret/pokefirered decompilation, and classify_step()
 *  follows the game's own collision checks (GetCollisionAtCoords() in
 *  src/event_object_movement.c), so a route planned on these maps is one the
 *  game allows.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldMap_H
#define PokemonAutomation_PokemonFRLG_OverworldMap_H

#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonFRLG_OverworldObjects.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  What decides where the player's next step can go.
struct PlayerState{
    TilePos position;
    uint8_t elevation = ELEVATION_GROUND;
    bool surfing = false;
};

//  Classification of movement result when taking a step.
enum class StepKind{
    BLOCKED,
    WALK,           //  moves one tile (on land, or on water while surfing)
    STOP_SURFING,   //  moves from water onto land and stops surfing
    START_SURF,     //  blocked, but facing water the player can Surf onto
};

//  What pressing `direction` once does to the player in `state`.
StepKind classify_step(const MapData& map, const PlayerState& state, Direction direction);

//  The same for a step that crosses into another map: from `state` on `from`
//  onto `destination` on `to`.
StepKind classify_step_onto(
    const MapData& from, const PlayerState& state, Direction direction,
    const MapData& to, const TilePos& destination
);

//  The player after a step of kind WALK, STOP_SURFING or START_SURF (the
//  Surf action moves the player onto the water).
PlayerState after_step(const MapData& map, const PlayerState& state, Direction direction, StepKind kind);
//  The player state resulting from stepping across map boundaries.
PlayerState after_step_onto(
    const MapData& from, const PlayerState& state,
    const MapData& to, const TilePos& destination, StepKind kind
);

//  A route between two tiles of one map stays on it if any route does
//  (plan_overworld_route()): `maps`' copy of that map alone, when
//  `start_map` and `goal_map` are the same and `maps` has others. The game
//  resets a map's objects when the player leaves it: 19.04b's below_ladder
//  went out of Victory Road 1F's cave mouth onto Route 23 and back in,
//  which a cheaper open stop by the mouth paid for, and the boulder pushed
//  onto the floor switch stood on (7, 18) again, where a leg ran into it
//  (console B, 2026-11-16, run 3). It is also the backtracking plans avoid.
std::optional<std::vector<MapData>> same_map_only(const std::vector<MapData>& maps, std::string_view start_map, std::string_view goal_map);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
