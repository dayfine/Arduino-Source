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


//  Direction enum from Pokemon navigation.
using Pokemon::Direction;
//  Grid tile coordinates.
using Pokemon::TilePos;
//  Classification of tile terrain.
using Pokemon::TileKind;
//  Parsed map representation.
using Pokemon::MapData;
//  Warp point connection.
using Pokemon::MapWarp;
//  Entity object on a map.
using Pokemon::MapObject;
//  Inter-map border connection.
using Pokemon::MapConnection;
//  Warp entry trigger definition.
using Pokemon::WarpEntry;
//  Finds warp located at specific tile coordinates.
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
//  - CLOSED_<side> can't be entered or left through that side. One with wild
//    encounters (a cave's) is listed in the map's region WILD_REGION, as its
//    glyph can't say so too.
//  - A warp entered by step_on (ladders, escalators) or cave_entrance warps
//    the player who steps onto its tile; walk_<direction> (exit mats, stairs
//    at the side of a room) warps the player standing on it who walks that
//    way.
//  A map without an elevation grid is all ground.

//  Whether the given point falls within the map bounds.
bool in_bounds(const MapData& map, const TilePos& point);
//  Out-of-bounds tiles are walls.
TileKind tile_at(const MapData& map, const TilePos& point);
//  Returns the elevation index at the given coordinate.
uint8_t elevation_at(const MapData& map, const TilePos& point);

//  Whether the tile at point is unwalkable wall terrain.
bool is_wall(const MapData& map, const TilePos& point);
//  Whether the tile at point is water terrain.
bool is_water(const MapData& map, const TilePos& point);
//  The map region of the tiles with wild encounters whose glyph isn't tall
//  grass.
extern const char WILD_REGION[];
//  Whether a step onto the tile may start a wild battle: tall grass, or a
//  tile in WILD_REGION.
bool is_tall_grass(const MapData& map, const TilePos& point);
//  Whether the tile at point is a one-way ledge.
bool is_ledge(const MapData& map, const TilePos& point);
//  Whether the tile itself refuses a step in `direction`: entering it, or
//  leaving it.
bool blocks_entering(const MapData& map, const TilePos& point, Direction direction);
//  Whether leaving the tile in the given direction is blocked.
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

//  Whether `object`, a trainer, may see the player on `tile`: up to its
//  sight ahead, any way it looks, short of a wall, from its tile or, for
//  one who wanders, from any tile it may wander to. False for an object
//  that isn't a trainer.
bool trainer_may_see(const MapData& map, const MapObject& object, const TilePos& tile);

//  Removes the objects the game hides once one of `flags` is set.
void hide_objects(MapData& map, const std::set<std::string>& flags);


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


//  Spinner tiles (Rocket Hideout B2F and B3F, Viridian Gym): pokefirered's
//  MB_SPIN_UP, MB_SPIN_DOWN, MB_SPIN_LEFT, MB_SPIN_RIGHT and
//  MB_STOP_SPINNING. The tile glyphs can't say so, so a map lists them in
//  these regions, as it does WILD_REGION.
//  "spin_north": MB_SPIN_UP.
extern const char SPIN_NORTH_REGION[];
//  "spin_south": MB_SPIN_DOWN.
extern const char SPIN_SOUTH_REGION[];
//  "spin_west": MB_SPIN_LEFT.
extern const char SPIN_WEST_REGION[];
//  "spin_east": MB_SPIN_RIGHT.
extern const char SPIN_EAST_REGION[];
//  "spin_stop": MB_STOP_SPINNING.
extern const char SPIN_STOP_REGION[];
//  The way the arrow on this tile slides the player, if it is a spinner.
std::optional<Direction> spin_direction(const MapData& map, const TilePos& point);
//  Whether this tile ends a slide (MB_STOP_SPINNING).
bool stops_spinning(const MapData& map, const TilePos& point);

//  Where a slide leaves the player, the way it last moved them, and how
//  many tiles it took.
struct SpinSlide{
    PlayerState end;
    Direction facing = Direction::NORTH;
    int tiles = 0;
};
//  The slide of a player who has just stepped onto the spinner `state`
//  stands on (pokefirered TryUpdatePlayerSpinDirection()): a tile at a
//  time the last arrow's way, turned by each arrow on the way, until a stop
//  tile, or a step the step rules refuse (a wall, a person: the game's
//  collision ends the forced movement there). Nothing if `state` isn't on a
//  spinner, if the arrows turn the player round in a loop, or if the slide
//  crosses a warp, which isn't modelled.
std::optional<SpinSlide> spin_slide(const MapData& map, const PlayerState& state);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
