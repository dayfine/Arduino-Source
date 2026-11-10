/*  Grid
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Tiles and directions on an overworld grid, shared by the games that
 *  move the player a tile at a time.
 *
 */

#ifndef PokemonAutomation_Pokemon_Grid_H
#define PokemonAutomation_Pokemon_Grid_H

#include <cstddef>
#include <functional>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  Row 0 is the top of a map, so SOUTH increases y.
enum class Direction{
    NORTH,
    EAST,
    SOUTH,
    WEST,
};
// Array containing all four cardinal directions.
constexpr Direction ALL_DIRECTIONS[] = {
    Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST,
};
// Returns the lowercase name string of a cardinal direction.
std::string direction_name(Direction direction);


// Grid coordinate with integer x and y components.
struct TilePos{
    int x = 0;
    int y = 0;

    TilePos next(Direction direction) const;
    bool operator==(const TilePos& other) const = default;
};
// Formats a TilePos as "(x, y)".
std::string to_string(const TilePos& pos);


//  One value per tile of a map, row by row from the top.
template <typename Cell>
struct Grid{
    int width = 0;
    int height = 0;
    std::vector<Cell> cells;

    bool contains(const TilePos& pos) const{
        return 0 <= pos.x && pos.x < width && 0 <= pos.y && pos.y < height;
    }
    //  `pos` must be inside the grid.
    const Cell& at(const TilePos& pos) const{
        return cells[(size_t)pos.y * width + pos.x];
    }
};
//  The cell at `pos` of `grid`, to change it; `pos` must be inside the grid.
//  A free function, so that Grid stays a plain value whose methods only read.
template <typename Cell>
Cell& cell_at(Grid<Cell>& grid, const TilePos& pos);

template <typename Cell>
Cell& cell_at(Grid<Cell>& grid, const TilePos& pos){
    return grid.cells[(size_t)pos.y * grid.width + pos.x];
}



} // namespace Pokemon
} // namespace PokemonAutomation


// Hash specialization for TilePos.
template <>
struct std::hash<PokemonAutomation::Pokemon::TilePos>{
    size_t operator()(const PokemonAutomation::Pokemon::TilePos& pos) const{
        return std::hash<long long>()(((long long)pos.x << 32) ^ (unsigned)pos.y);
    }
};


#endif
