/*  Grid
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


const char* direction_name(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return "north";
    case Direction::EAST:
        return "east";
    case Direction::SOUTH:
        return "south";
    case Direction::WEST:
        return "west";
    }
    return "?";
}


TilePos TilePos::next(Direction direction) const{
    switch (direction){
    case Direction::NORTH:
        return {x, y - 1};
    case Direction::EAST:
        return {x + 1, y};
    case Direction::SOUTH:
        return {x, y + 1};
    case Direction::WEST:
        return {x - 1, y};
    }
    return *this;
}

std::string to_string(const TilePos& pos){
    return "(" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ")";
}



}
}
