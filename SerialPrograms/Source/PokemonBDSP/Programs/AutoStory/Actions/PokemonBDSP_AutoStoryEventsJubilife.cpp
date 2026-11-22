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
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRoutePlanner.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryRouteTiming.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryRouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryWalkDpad.h"
#include "PokemonBDSP_AutoStoryTalks.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryWalks.h"
#include "PokemonBDSP_AutoStoryWildBattle.h"
#include "PokemonBDSP_AutoStoryEvents.h"

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

//  The walk onto the row below a wanderer can end a row low: on 00.05b's
//  east 8 to (55, 34), Dawn grazed school_kid_m_5 (51, 33..35) and BDSP's
//  collider slid her onto row 35 (lab-B 2026-11-22), so every round asked
//  row 35 and mart_clown on (55, 33) never answered. Down to the wall
//  `wall_tiles` below the row and as many back up puts us on the row again
//  from either.
void back_onto_row(VideoStream& stream, ProControllerContext& context, size_t wall_tiles, const std::string& name, Direction& facing){
    turn_to(stream, context, facing, Direction::SOUTH);
    const WalkResult down = walk_tiles_dpad(stream, context, run_from_wild_battle, route_stick(Direction::SOUTH),
        wall_tiles + 1, std::format("Down to the wall below {}'s row", name));
    turn_to(stream, context, facing, Direction::NORTH);
    const WalkResult up = walk_tiles_dpad(stream, context, run_from_wild_battle, route_stick(Direction::NORTH),
        wall_tiles, std::format("Back up to the row below {}", name));
    stream.log(std::format("[Talk] {}: back onto the row from the wall {} tiles below it: {} tiles down, {} up.",
        name, wall_tiles, down.moved_chunks, up.moved_chunks));
}

std::optional<Pokemon::StoryFailure> finish_wanderer_talk(
    VideoStream& stream, ProControllerContext& context, Direction& facing,
    int at, size_t round, const std::string& name
){
    stream.log(std::format("[Talk] {}: answered in round {}, {} tiles east of home.", name, round, at));
    finish_dialogue(stream, context, BUTTON_A, name);
    return_below_home(stream, context, facing, at, name);
    return std::nullopt;
}

std::optional<Pokemon::StoryFailure> talk_to_wanderer(StoryEnv& console, BdspRunState& state, const BdspStep& step){
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
    const std::optional<size_t> anchor = wanderer_row_anchor(state.pose);
    int at = 0;
    for (size_t round = 0; round < 6; round++){
        //  A first round in vain may have asked from a row low: back onto
        //  the row by the wall below it, where the map has one close.
        if (round == 1 && anchor && at == 0){
            back_onto_row(stream, context, *anchor, step.name, facing);
        }
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


std::optional<size_t> wanderer_row_anchor(const Pokemon::GridPose& pose){
    if (pose.tiles.size() != 1){
        return std::nullopt;
    }
    const StoryWorld* world = story_plan_world(pose.map, pose.map);
    if (world == nullptr){
        return std::nullopt;
    }
    const Pokemon::TilePos start = story_to_world(*world, pose.map, pose.tiles[0]);
    const WallHit wall = slide_to_wall(*world, start, Direction::SOUTH);
    if (!wall.anchored || wall.tiles == 0 || wall.tiles > WANDERER_ANCHOR_MOST_TILES){
        return std::nullopt;
    }
    //  Someone wandering onto the column would stop the walk short of it.
    Pokemon::TilePos at = start;
    for (size_t i = 0; i < wall.tiles; i++){
        at = at.next(Direction::SOUTH);
        if (wandering_hazard(*world, at)){
            return std::nullopt;
        }
    }
    return wall.tiles;
}

void add_jubilife_events(StoryEvents& events){
    events.emplace("talk_to_wanderer", talk_to_wanderer);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
