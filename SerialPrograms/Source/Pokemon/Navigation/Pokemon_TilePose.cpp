/*  Tile Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Pokemon_TilePose.h"

namespace PokemonAutomation{
namespace Pokemon{


TilePose::TilePose(std::vector<TilePos> tiles, std::optional<Direction> facing)
    : tiles(std::move(tiles))
    , facing(facing)
{}

std::string TilePose::describe() const{
    std::string text;
    for (const TilePos& tile : tiles){
        text += (text.empty() ? "" : " ") + to_string(tile);
    }
    if (facing){
        text += (text.empty() ? "facing " : " facing ") + std::string(direction_name(*facing));
    }
    return text;
}

std::vector<TilePos> tiles_of(const Pose& pose){
    const TilePose* tile_pose = dynamic_cast<const TilePose*>(pose.where.get());
    return tile_pose ? tile_pose->tiles : std::vector<TilePos>{};
}



}
}
