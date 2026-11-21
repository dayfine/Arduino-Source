/*  Tile Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
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
        text += std::format("{}facing {}", text.empty() ? "" : " ", direction_name(*facing));
    }
    return text;
}

std::vector<TilePos> tiles_of(const Pose& pose){
    const TilePose* position = dynamic_cast<const TilePose*>(pose.where.get());
    return position ? position->tiles : std::vector<TilePos>{};
}

std::optional<Direction> facing_of(const Pose& pose){
    const TilePose* position = dynamic_cast<const TilePose*>(pose.where.get());
    return position ? position->facing : std::nullopt;
}

Pose tile_pose(std::string map, std::vector<TilePos> tiles, std::optional<Direction> facing){
    Pose pose;
    pose.map = std::move(map);
    pose.certainty = tiles.empty() ? PoseCertainty::UNKNOWN
        : tiles.size() == 1 ? PoseCertainty::TILE : PoseCertainty::SET;
    pose.where = std::make_shared<const TilePose>(std::move(tiles), facing);
    return pose;
}



} // namespace Pokemon
} // namespace PokemonAutomation
