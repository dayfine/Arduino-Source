/*  AutoStory Scripted Rooms
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryRooms.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


bool is_center_variant(std::string_view variant){
    return variant == CENTER_ENTER_VARIANT || variant == CENTER_EXIT_VARIANT;
}

const std::vector<RoomLeg>& center_enter_legs(){
    static const std::vector<RoomLeg> LEGS{
        {Pokemon::Direction::NORTH, 1, true},
        {Pokemon::Direction::NORTH, 4, false},
    };
    return LEGS;
}
const std::vector<RoomLeg>& center_exit_legs(){
    static const std::vector<RoomLeg> LEGS{
        {Pokemon::Direction::SOUTH, 4, false},
        {Pokemon::Direction::SOUTH, 1, true},
    };
    return LEGS;
}
const std::vector<RoomLeg>& lift_in_legs(){
    static const std::vector<RoomLeg> LEGS{
        {Pokemon::Direction::NORTH, 1, true},
        {Pokemon::Direction::NORTH, 2, false},
        {Pokemon::Direction::WEST, 2, false},
        {Pokemon::Direction::NORTH, 0, false},
    };
    return LEGS;
}
const std::vector<RoomLeg>& lift_out_legs(){
    static const std::vector<RoomLeg> LEGS{
        {Pokemon::Direction::EAST, 2, false},
        {Pokemon::Direction::SOUTH, 2, false},
        {Pokemon::Direction::SOUTH, 1, true},
    };
    return LEGS;
}
const std::vector<RoomLeg>& center_legs(std::string_view variant){
    static const std::vector<RoomLeg> NONE;
    if (variant == CENTER_ENTER_VARIANT){
        return center_enter_legs();
    }
    if (variant == CENTER_EXIT_VARIANT){
        return center_exit_legs();
    }
    return NONE;
}


void walk_room_legs(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RoomLeg>& legs, std::optional<Pokemon::Direction> facing
){
    for (const RoomLeg& leg : legs){
        if (leg.tiles == 0){
            OverworldWalkSteps::turn_to(console, context, leg.direction);
            facing = leg.direction;
            continue;
        }
        for (int tile = 0; tile < leg.tiles; tile++){
            step_once(console, context, leg.direction, facing);
            facing = leg.direction;
        }
        if (leg.warps){
            RouteStep step;
            step.direction = leg.direction;
            step.tiles = 1;
            step.warps = true;
            OverworldWalkSteps::wait_out_warp(console, context, step);
        }
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
