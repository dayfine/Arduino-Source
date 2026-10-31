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

namespace PokemonAutomation{
namespace Pokemon{


//  Row 0 is the top of a map, so SOUTH increases y.
enum class Direction{
    NORTH,
    EAST,
    SOUTH,
    WEST,
};
constexpr Direction ALL_DIRECTIONS[] = {
    Direction::NORTH, Direction::EAST, Direction::SOUTH, Direction::WEST,
};
const char* direction_name(Direction direction);


struct TilePos{
    int x = 0;
    int y = 0;

    TilePos next(Direction direction) const;
    bool operator==(const TilePos& other) const = default;
};
std::string to_string(const TilePos& pos);



}
}


template <>
struct std::hash<PokemonAutomation::Pokemon::TilePos>{
    size_t operator()(const PokemonAutomation::Pokemon::TilePos& pos) const{
        return std::hash<long long>()(((long long)pos.x << 32) ^ (unsigned)pos.y);
    }
};


#endif
