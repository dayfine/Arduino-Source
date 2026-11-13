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
bool ask_north(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::NORTH);
    for (size_t ask = 0; ask < 2; ask++){
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_YELLOW);
        if (wait_until(stream, context, 1500ms, {{dialog}}) == 0){
            return true;
        }
    }
    return false;
}
//  One tile along the row, east (+1) or west (-1).
void step_along(VideoStream& stream, ProControllerContext& context, int east, const std::string& label){
    route_turn(stream, context, east > 0 ? Direction::EAST : Direction::WEST);
    walk_tiles_dpad(stream, context, run_from_wild_battle, {east > 0 ? 1.0 : -1.0, 0}, 1, label);
}
Pokemon::Outcome talk_to_wanderer(StoryExecutor& executor, Pokemon::RunState&, const Pokemon::StoryStep& step){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    const std::string label = "Along the row below " + step.name;
    //  Home, east, west, west, east: each move one tile on the D-pad.
    const int moves[] = {0, 1, -1, -1, 1};
    int at = 0;
    for (size_t round = 0; round < 6; round++){
        for (int move : moves){
            if (move != 0){
                step_along(stream, context, move, label);
                at += move;
            }
            if (!ask_north(stream, context)){
                continue;
            }
            stream.log(std::format("[Talk] {}: answered in round {}, {} tiles east of home.", step.name, round + 1, at));
            finish_dialogue(stream, context, BUTTON_A, step.name);
            for (; at != 0; at += at > 0 ? -1 : 1){
                step_along(stream, context, at > 0 ? -1 : 1, "Back below " + step.name + "'s home");
            }
            route_turn(stream, context, Direction::NORTH);
            return Pokemon::outcome_ok();
        }
    }
    fail_step(stream, step.name + ": no answer from any tile below his range.");
}

} // namespace


void add_jubilife_events(StoryEvents& events){
    events.emplace("talk_to_wanderer", talk_to_wanderer);
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
