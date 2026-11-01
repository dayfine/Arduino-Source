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
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryWorld_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryWorld_H

#include <optional>
#include <string>
#include <vector>
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


class StoryWorld{
public:
    //  Places `map` under `name` with its tile (0,0) at `offset`. Offsets are
    //  relative to each other and may be negative. Maps may overlap only
    //  where both are walls with nobody on them; the map placed first holds
    //  those tiles. Throws InternalProgramError if the name is taken or the
    //  maps conflict. World tile (0,0) is the top-left corner of all the
    //  maps together.
    void add(std::string name, const Pokemon::MapData& map, const Pokemon::TilePos& offset);

    int width() const{ return m_cells.width; }
    int height() const{ return m_cells.height; }

    //  Whether a map holds this world tile.
    bool covered(const Pokemon::TilePos& pos) const;
    //  The tile, or nothing off the maps.
    std::optional<Pokemon::TileKind> tile(const Pokemon::TilePos& pos) const;
    //  Whether someone or something stands still on this tile, or it was
    //  blocked with block().
    bool occupied(const Pokemon::TilePos& pos) const;

    //  Whether the player can stand on this tile.
    bool standable(const Pokemon::TilePos& pos) const;
    //  Where one step from `from` lands, or nothing if the player is stopped.
    //  A step onto a ledge the arrow's way hops to the tile past it.
    std::optional<Pokemon::TilePos> step(const Pokemon::TilePos& from, Pokemon::Direction direction) const;
    //  Whether a step from `pos` is refused by a tile that holds the player
    //  in place. Doors and tiles off the maps are not walls: walking into a
    //  door enters the building, and ground off the maps may be an exit.
    bool at_wall(const Pokemon::TilePos& pos, Pokemon::Direction direction) const;

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
    //  "Route 201 (45, 3)", or "world (x, y)" if no map holds it.
    std::string describe(const Pokemon::TilePos& world) const;

private:
    struct Placed{
        std::string name;
        Pokemon::Grid<Pokemon::TileKind> tiles;
        std::vector<Pokemon::TilePos> occupied;     //  local tiles
        Pokemon::TilePos offset;
    };
    struct Cell{
        bool covered = false;
        Pokemon::TileKind tile = Pokemon::TileKind::WALL;
        bool occupied = false;
    };

    void check_overlap(const std::string& name, const Placed& map) const;
    void rebuild();
    const Cell* cell(const Pokemon::TilePos& pos) const;
    Cell* cell(const Pokemon::TilePos& pos);

    std::vector<Placed> m_maps;
    Pokemon::TilePos m_origin;      //  offset of world tile (0,0)
    Pokemon::Grid<Cell> m_cells;
    std::vector<Pokemon::TilePos> m_blocked;    //  world tiles, kept across add()
    std::vector<Pokemon::TilePos> m_uncovered;
};



}
}
}
#endif
