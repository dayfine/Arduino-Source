/*  BDSP AutoStory - Old story steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP_AutoStory_OldStorySteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


StoryStep r75_old_gate_exit(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "out_of_oreburgh_gate";
    step.legs = {{.direction = Direction::WEST, .tiles = 8}};
    step.variant = "to_map";
    step.pose_after = Pokemon::tile_pose("Route203", {TilePos{55, 9}}, Direction::WEST);
    return step;
}
Pose r75_pond_pocket(){
    std::vector<TilePos> tiles;
    for (int y = 6; y <= 13; y++){
        for (int x = 49; x <= 54; x++){
            tiles.push_back({x, y});
        }
    }
    return Pokemon::tile_pose("Route203", tiles);
}

StoryStep r81_old_cave_exit(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "out_of_the_ravaged_path";
    step.legs = {{.direction = Direction::SOUTH, .tiles = 4}};
    step.variant = "to_wall";
    step.pose_after = Pokemon::tile_pose("Route204North", {TilePos{20, 26}}, Direction::SOUTH);
    return step;
}
Pose r81_below_the_mouth(){
    return Pokemon::tile_pose("Route204North", {TilePos{20, 27}, TilePos{20, 28}, TilePos{20, 29}, TilePos{20, 30}});
}

StoryStep old_mine_hall_19(std::string name, Direction direction){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = std::move(name);
    step.legs = {{.direction = direction, .tiles = 19}};
    return step;
}

StoryStep old_coal_room_crate_cut(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "to_roarks_corner";
    step.legs = {
        {.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 2},
        {.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 5},
        {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
        {.direction = Direction::SOUTH, .tiles = 2}, {.direction = Direction::WEST, .tiles = 11},
    };
    return step;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
