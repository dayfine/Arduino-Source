/*  BDSP AutoStory - Story world
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Several story maps placed side by side on one world grid, so one plan can
 *  cross from a route into a town. What the tiles mean for walking in BDSP:
 *  open ground and tall grass are stood on; a ledge is hopped the arrow's
 *  way; walls, water and signs stop the player; a door is walked into. A
 *  person or an item standing still blocks their tile; a wanderer does not
 *  (the walker waits for them). Tiles no map covers are off the maps: a
 *  plan may not end there, but walking into one is not a wall, since it may
 *  be an exit.
 *
 *  The same maps are also kept as connected maps (Pokemon::ConnectedMaps),
 *  joined where they meet on the world grid, for planning on a map and a
 *  tile instead of the grid.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryWorld_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryWorld_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Navigation/Pokemon_ConnectedMaps.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A tile of one of the maps, in that map's own coordinates.
struct MapTile{
    std::string map;
    Pokemon::TilePos pos;
};


//  Composite coordinate space combining multiple contiguous maps into one
//  traversable overworld representation.
class StoryWorld{
public:
    //  Places `map` under `name` with its tile (0,0) at `offset`. Offsets are
    //  relative to each other and may be negative. Maps may overlap only
    //  where both are walls with nobody on them; the map placed first holds
    //  those tiles. Throws InternalProgramError if the name is taken or the
    //  maps conflict. World tile (0,0) is the top-left corner of all the
    //  maps together.
    void add(std::string name, const Pokemon::MapData& map, const Pokemon::TilePos& offset);

    //  The tile, or nothing off the maps.
    std::optional<Pokemon::TileKind> tile(const Pokemon::TilePos& pos) const;
    //  Who or what stands still on a tile: one of the maps' people, or a
    //  tile blocked with block(). NOBODY is false, the others true.
    enum Occupant{
        NOBODY = 0,
        PERSON,
        BLOCKED,
    };
    //  Recovery also excludes wandering areas and declared warps.
    enum class OccupancyUse{ MOVEMENT, WALL_RECOVERY };
    Occupant occupied(const Pokemon::TilePos& pos, OccupancyUse use = OccupancyUse::MOVEMENT) const;

    //  Blocks a tile found in the way on the console. Tiles off the maps
    //  are left alone.
    void block(const Pokemon::TilePos& pos);
    //  Takes a tile off the maps: a tile a walk must not cross or stop on
    //  that the game lets us walk (a trainer's line of sight).
    void uncover(const Pokemon::TilePos& pos);

    //  Throws InternalProgramError for a map name it does not have.
    Pokemon::TilePos to_world(const std::string& map, const Pokemon::TilePos& local) const;
    //  The map that holds a world tile, or nothing between maps.
    std::optional<MapTile> to_local(const Pokemon::TilePos& world) const;

    friend Pokemon::ConnectedMaps connected_maps(const StoryWorld& world);
    friend std::vector<Pokemon::TilePos> wandering_hazards(const StoryWorld& world);
    friend bool wandering_hazard(const StoryWorld& world, const Pokemon::TilePos& pos);
    friend bool recovery_hazard(const StoryWorld& world, const Pokemon::TilePos& pos);

private:
    struct Placed{
        std::string name;
        Pokemon::Grid<Pokemon::TileKind> tiles;
        std::vector<Pokemon::TilePos> occupied;     //  local tiles
        std::vector<Pokemon::TileBox> wander;       //  recovery hazards, not terrain
        std::vector<Pokemon::TilePos> warps;
        Pokemon::TilePos offset;
    };
    struct Cell{
        bool covered = false;
        Pokemon::TileKind tile = Pokemon::TileKind::WALL;
        Occupant occupant = NOBODY;
        bool recovery_hazard = false;
        bool wander_hazard = false;
    };

    void check_overlap(const Placed& map) const;
    //  The first tile of `map` that `placed` holds as anything but a wall
    //  with nobody on it, or nothing.
    static std::optional<Pokemon::TilePos> conflict(const Placed& map, const Placed& placed);
    void rebuild();
    void paint(const Placed& placed, const Pokemon::TilePos& corner);
    const Cell* cell(const Pokemon::TilePos& pos) const;
    Cell* cell(const Pokemon::TilePos& pos);

    std::vector<Placed> m_maps;
    std::vector<Pokemon::MapData> m_connected;
    Pokemon::TilePos m_origin;      //  offset of world tile (0,0)
    Pokemon::Grid<Cell> m_cells;
    std::vector<Pokemon::TilePos> m_blocked;    //  world tiles, kept across add()
    std::vector<Pokemon::TilePos> m_uncovered;
};


//  The world's maps as added, under their names in it, joined where they
//  meet. Tiles blocked or taken off the maps are not in them. The view
//  lasts until the world's next add().
Pokemon::ConnectedMaps connected_maps(const StoryWorld& world);
//  A temporary world would be gone before the view is used.
Pokemon::ConnectedMaps connected_maps(StoryWorld&& world) = delete;

//  The rules of walking on a story world, from its tiles.

//  All tiles where someone wanders on the maps, in world coordinates.
std::vector<Pokemon::TilePos> wandering_hazards(const StoryWorld& world);
//  Whether a world tile is within a wandering hazard box.
bool wandering_hazard(const StoryWorld& world, const Pokemon::TilePos& pos);
//  Whether a world tile is a recovery hazard (wander box or warp).
bool recovery_hazard(const StoryWorld& world, const Pokemon::TilePos& pos);
//  "Route 201 (45, 3)", or "world (x, y)" if no map holds it.
std::string describe(const StoryWorld& world, const Pokemon::TilePos& pos);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
