/*  BDSP AutoStory - Old story steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include <vector>
#include "PokemonBDSP_AutoStory_OldStorySteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::TilePos;


BdspStep r75_old_gate_exit(){
    return BdspStep{
        .name = "out_of_oreburgh_gate",
        .action = "legs",
        .args = LegsStepArgs{
            .legs = {{.direction = Direction::WEST, .tiles = 8}},
            .fight_wild = false,
            .variant = "to_map",
        },
        .pose_after = GridPose{.map = "Route203", .tiles = {TilePos{55, 9}}, .facing = Direction::WEST},
    };
}
GridPose r75_pond_pocket(){
    std::vector<TilePos> tiles;
    for (int y = 6; y <= 13; y++){
        for (int x = 49; x <= 54; x++){
            tiles.push_back({x, y});
        }
    }
    return GridPose{.map = "Route203", .tiles = std::move(tiles)};
}

BdspStep r81_old_cave_exit(){
    return BdspStep{
        .name = "out_of_the_ravaged_path",
        .action = "legs",
        .args = LegsStepArgs{
            .legs = {{.direction = Direction::SOUTH, .tiles = 4}},
            .fight_wild = false,
            .variant = "to_wall",
        },
        .pose_after = GridPose{.map = "Route204North", .tiles = {TilePos{20, 26}}, .facing = Direction::SOUTH},
    };
}
GridPose r81_below_the_mouth(){
    return GridPose{.map = "Route204North", .tiles = {TilePos{20, 27}, TilePos{20, 28}, TilePos{20, 29}, TilePos{20, 30}}};
}

BdspStep old_mine_hall_19(std::string name, Direction direction){
    return BdspStep{
        .name = std::move(name),
        .action = "legs",
        .args = LegsStepArgs{
            .legs = {{.direction = direction, .tiles = 19}},
            .fight_wild = false,
            .variant = "legs",
        },
    };
}

BdspStep old_coal_room_crate_cut(){
    return BdspStep{
        .name = "to_roarks_corner",
        .action = "legs",
        .args = LegsStepArgs{
            .legs = {
                {.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 2},
                {.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 5},
                {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
                {.direction = Direction::SOUTH, .tiles = 2}, {.direction = Direction::WEST, .tiles = 11},
            },
            .fight_wild = false,
            .variant = "legs",
        },
    };
}

std::vector<BdspStep> old_round_mom_to_the_mat(){
    const GridPose mat{.map = "PlayerHouse1F", .tiles = {TilePos{6, 9}}, .facing = Direction::SOUTH};
    const RouteStepArgs route_args{.target = mat, .fight_wild = false, .variant = "route"};
    BdspStep around{.name = "around_mom", .action = "route", .args = route_args};
    BdspStep step{.name = "to_moms_call", .action = "step", .args = StepMoveArgs{.direction = Direction::SOUTH}};
    return {std::move(around), std::move(step)};
}

BdspStep old_tap_into_the_gate(){
    return BdspStep{
        .name = "into_oreburgh_gate",
        .action = "step",
        .args = StepMoveArgs{.direction = Direction::WEST, .variant = "to_banner"},
        .pose_after = GridPose{.map = "OreburghGate", .tiles = {TilePos{27, 22}, TilePos{26, 22}}, .facing = Direction::WEST},
    };
}

BdspStep old_west_along_the_field(){
    return BdspStep{
        .name = "west_along_the_field",
        .action = "legs",
        .args = LegsStepArgs{
            .legs = {{.direction = Direction::WEST, .tiles = 8}},
            .fight_wild = false,
            .variant = "legs",
        },
    };
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
