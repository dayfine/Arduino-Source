/*  Tile Pose
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The position a 2D tile game (BDSP, FRLG) gives the story-event engine:
 *  the tiles the player may stand on and the way they face. The engine
 *  (Pokemon_StoryEvents.h) does not know tiles; games include this header.
 *
 */

#ifndef PokemonAutomation_Pokemon_TilePose_H
#define PokemonAutomation_Pokemon_TilePose_H

#include <optional>
#include <string>
#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_StoryEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One of `tiles`, facing `facing` if known.
struct TilePose : public Position{
    std::vector<TilePos> tiles;
    std::optional<Direction> facing;

    explicit TilePose(std::vector<TilePos> tiles, std::optional<Direction> facing = std::nullopt);
    //  "(10, 20) facing north": the tiles, then the facing, each after a space.
    virtual std::string describe() const override;
};

//  The tiles of a pose: empty when the pose holds no position, or one that
//  is not a tile pose.
std::vector<TilePos> tiles_of(const Pose& pose);



}
}
#endif
