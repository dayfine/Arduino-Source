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
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using Pokemon::Direction;
using Pokemon::TilePos;
using Pokemon::TileKind;
using Pokemon::MapData;
using Pokemon::MapWarp;
using Pokemon::MapObject;
using Pokemon::MapConnection;
using Pokemon::WarpEntry;
using Pokemon::warp_at;


//  Elevations with a meaning of their own in the game's rules
//  (IsElevationMismatchAt() and ObjectEventUpdateElevation() in
//  pokefirered).
//  Stairs and ladders: they join every elevation.
constexpr uint8_t ELEVATION_ANY = 0;
//  Bridges and the like: they don't change the player's elevation.
constexpr uint8_t ELEVATION_IGNORED = 15;
//  Ordinary ground: most walkable tiles in the decompilation's map layouts.
//  Surf starts and ends here (CanStopSurfing(), and the Surf field move
//  checks the player's elevation is 3).
constexpr uint8_t ELEVATION_GROUND = 3;


//  How FRLG reads a map's tiles (Pokemon::TileKind) and warps:
//  - WALL, DOOR, SIGN and the ledges are walls. A DOOR warps the player who
//    walks north into it; a SIGN shows its message to the player who walks
//    into it moving north or south (signposts, Center and Mart signs;
//    pokefirered TrySetUpWalkIntoSignpostScript()). Ledges are hopped the
//    way they face, which isn't modelled, so the planner avoids them.
//  - WATER is entered only by starting to Surf.
//  - CLOSED_<side> can't be entered or left through that side.
//  - A warp entered by step_on (ladders, escalators) or cave_entrance warps
//    the player who steps onto its tile; walk_<direction> (exit mats, stairs
//    at the side of a room) warps the player standing on it who walks that
//    way.
//  A map without an elevation grid is all ground.

bool in_bounds(const MapData& map, const TilePos& point);
//  Out-of-bounds tiles are walls.
TileKind tile_at(const MapData& map, const TilePos& point);
uint8_t elevation_at(const MapData& map, const TilePos& point);

bool is_wall(const MapData& map, const TilePos& point);
bool is_water(const MapData& map, const TilePos& point);
bool is_tall_grass(const MapData& map, const TilePos& point);
bool is_ledge(const MapData& map, const TilePos& point);
//  Whether the tile itself refuses a step in `direction`: entering it, or
//  leaving it.
bool blocks_entering(const MapData& map, const TilePos& point, Direction direction);
bool blocks_leaving(const MapData& map, const TilePos& point, Direction direction);

//  Whether the game takes this tile's warp on stepping onto it (a ladder or
//  cave entrance).
bool warps_on_step(const MapData& map, const TilePos& point);
//  A door, entered by walking north into it.
bool is_door(const MapData& map, const TilePos& point);
//  The direction that takes the exit mat on this tile, if it is one.
std::optional<Direction> exit_mat(const MapData& map, const TilePos& point);
//  Arriving on a door or cave entrance, the game walks the player one
//  tile south, out of it.
bool walks_out_on_arrival(const MapData& map, const TilePos& point);

//  The object standing, or maybe wandering, on this tile.
const MapObject* object_at(const MapData& map, const TilePos& point);

//  Removes the objects the game hides once one of `flags` is set.
void hide_objects(MapData& map, const std::set<std::string>& flags);


//  What decides where the player's next step can go.
struct PlayerState{
    TilePos position;
    uint8_t elevation = ELEVATION_GROUND;
    bool surfing = false;
};

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
PlayerState after_step_onto(
    const MapData& from, const PlayerState& state,
    const MapData& to, const TilePos& destination, StepKind kind
);



}
}
}
#endif
