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
using Pokemon::GridPose;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;


StoryStep r75_old_gate_exit(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "out_of_oreburgh_gate";
    step.action = "to_map";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = Direction::WEST, .tiles = 8}};
    step.args["fight_wild"] = false;
    step.pose_after = GridPose{.map = "Route203", .tiles = {TilePos{55, 9}}, .facing = Direction::WEST};
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
    step.action = "to_wall";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = Direction::SOUTH, .tiles = 4}};
    step.args["fight_wild"] = false;
    step.pose_after = GridPose{.map = "Route204North", .tiles = {TilePos{20, 26}}, .facing = Direction::SOUTH};
    return step;
}
Pose r81_below_the_mouth(){
    return Pokemon::tile_pose("Route204North", {TilePos{20, 27}, TilePos{20, 28}, TilePos{20, 29}, TilePos{20, 30}});
}

StoryStep old_mine_hall_19(std::string name, Direction direction){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = std::move(name);
    step.action = "legs";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = direction, .tiles = 19}};
    step.args["fight_wild"] = false;
    return step;
}

StoryStep old_coal_room_crate_cut(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "to_roarks_corner";
    step.action = "legs";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{
        {.direction = Direction::EAST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 2},
        {.direction = Direction::WEST, .tiles = 1}, {.direction = Direction::SOUTH, .tiles = 5},
        {.direction = Direction::EAST, .tiles = 14}, {.direction = Direction::SOUTH, .tiles = 10},
        {.direction = Direction::SOUTH, .tiles = 2}, {.direction = Direction::WEST, .tiles = 11},
    };
    step.args["fight_wild"] = false;
    return step;
}

std::vector<StoryStep> old_round_mom_to_the_mat(){
    StoryStep around;
    around.kind = StoryStepKind::ROUTE;
    around.name = "around_mom";
    around.action = "route";
    around.args["target"] = GridPose{.map = "PlayerHouse1F", .tiles = {TilePos{6, 9}}, .facing = Direction::SOUTH};
    around.args["fight_wild"] = false;
    StoryStep step;
    step.kind = StoryStepKind::STEP;
    step.name = "to_moms_call";
    step.action = "step";
    step.args["direction"] = Direction::SOUTH;
    return {around, step};
}

StoryStep old_tap_into_the_gate(){
    StoryStep step;
    step.kind = StoryStepKind::STEP;
    step.name = "into_oreburgh_gate";
    step.action = "to_banner";
    step.args["direction"] = Direction::WEST;
    step.pose_after = GridPose{.map = "OreburghGate", .tiles = {TilePos{27, 22}, TilePos{26, 22}}, .facing = Direction::WEST};
    return step;
}

StoryStep old_west_along_the_field(){
    StoryStep step;
    step.kind = StoryStepKind::LEGS;
    step.name = "west_along_the_field";
    step.action = "legs";
    step.args["legs"] = std::vector<Pokemon::StoryLeg>{{.direction = Direction::WEST, .tiles = 8}};
    step.args["fight_wild"] = false;
    return step;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
