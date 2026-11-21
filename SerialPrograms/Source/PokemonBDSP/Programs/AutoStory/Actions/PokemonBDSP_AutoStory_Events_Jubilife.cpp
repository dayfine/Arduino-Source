/*  BDSP AutoStory - Events: Jubilife City (00.05b)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Jubilife beat of 00.05b the story table cannot plan: the talk to a
 *  wandering clown.
 *
 */

#include <format>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  00.05b: a talk to someone who wanders a tile either side of home along a
//  row (the clowns by the Mart and the Poketch Company, WANDER_AROUND with
//  x range 1). Waiting under home alone missed him for a minute (r66: he
//  stood a tile east), so walk the row below his range, a whole D-pad tile
//  at a time, asking north from each tile. His question's answer is Yes, so
//  A answers and takes the coupon; then back under his home tile.
//
//  Keep track of facing so route_turn() is never sent in the direction we
//  already face: a same-direction turn tap walks a tile in BDSP (which
//  marched company_clown north into the wall when round 1 started facing
//  north and every later round repeated move=0 after ask_north).
void turn_to(VideoStream& stream, ProControllerContext& context, Direction& facing, Direction target){
    if (facing != target){
        route_turn(stream, context, target);
        facing = target;
    }
}

bool ask_ahead(VideoStream& stream, ProControllerContext& context){
    for (size_t ask = 0; ask < 2; ask++){
        pbf_press_button(context, BUTTON_A, /*hold=*/80ms, /*release=*/0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            return true;
        }
    }
    return false;
}

bool step_along(VideoStream& stream, ProControllerContext& context, Direction& facing, int east, const std::string& label){
    const Direction dir = east > 0 ? Direction::EAST : Direction::WEST;
    turn_to(stream, context, facing, dir);
    const WalkResult walked = walk_tiles_dpad(stream, context, run_from_wild_battle, {east > 0 ? 1.0 : -1.0, 0}, 1, label);
    return walked.moved_chunks > 0;
}

void return_below_home(VideoStream& stream, ProControllerContext& context, Direction& facing, int at, const std::string& name){
    const std::string back = std::format("Back below {}'s home", name);
    for (size_t tries = 0; at != 0 && tries < 6; tries++){
        const int step = at > 0 ? -1 : 1;
        if (step_along(stream, context, facing, step, back)){
            at += step;
        }
    }
    turn_to(stream, context, facing, Direction::NORTH);
}

Pokemon::Outcome finish_wanderer_talk(
    VideoStream& stream, ProControllerContext& context, Direction& facing,
    int at, size_t round, const std::string& name
){
    stream.log(std::format("[Talk] {}: answered in round {}, {} tiles east of home.", name, round, at));
    finish_dialogue(stream, context, BUTTON_A, name);
    return_below_home(stream, context, facing, at, name);
    return Pokemon::outcome_ok();
}

Pokemon::Outcome talk_to_wanderer(StoryEnv& console, Pokemon::RunState&, const Pokemon::StoryStep& step){
    VideoStream& stream = console.env.console;
    ProControllerContext& context = console.context;
    const std::string label = std::format("Along the row below {}", step.name);
    //  Check ahead first in case the previous route ended facing north; then
    //  turn south->north so facing is known north without stepping north.
    if (ask_ahead(stream, context)){
        Direction facing = Direction::NORTH;
        return finish_wanderer_talk(stream, context, facing, 0, 0, step.name);
    }
    route_turn(stream, context, Direction::SOUTH);
    route_turn(stream, context, Direction::NORTH);
    Direction facing = Direction::NORTH;
    if (ask_ahead(stream, context)){
        return finish_wanderer_talk(stream, context, facing, 0, 1, step.name);
    }
    const int targets[] = {1, 0, -1, 0};
    int at = 0;
    for (size_t round = 0; round < 6; round++){
        for (int target : targets){
            if (at != target){
                const int move = target > at ? 1 : -1;
                if (step_along(stream, context, facing, move, label)){
                    at += move;
                }else if (ask_ahead(stream, context)){
                    return finish_wanderer_talk(stream, context, facing, at, round + 1, step.name);
                }
            }
            turn_to(stream, context, facing, Direction::NORTH);
            if (ask_ahead(stream, context)){
                return finish_wanderer_talk(stream, context, facing, at, round + 1, step.name);
            }
        }
    }
    fail_step(stream, std::format("{}: no answer from any tile below his range.", step.name));
}

} // namespace


void add_jubilife_events(StoryEvents& events){
    events.emplace("talk_to_wanderer", talk_to_wanderer);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
