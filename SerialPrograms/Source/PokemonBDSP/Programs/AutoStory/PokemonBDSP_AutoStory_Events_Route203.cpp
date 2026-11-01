/*  BDSP AutoStory - Events: Route 203 (01.02, 01.06)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Segment 01's walks along Route 203: east to the Oreburgh Gate and through
 *  it to Oreburgh, and back west to Jubilife, as they ran on the console
 *  (2026-10-29 to 10-31).
 *
 */

#include <cstdlib>
#include "CommonFramework/GlobalAutoPaths.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Globals.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_PlannedRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  01.02: east along Route 203 into the Oreburgh Gate, fighting whoever stops us.
Pokemon::Outcome route_203_east_to_the_gate(StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep&){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    // Route 203 only goes east, over three stretches joined by stairs, and
    // the Gate's opening is in the cliff at its north-east end. Who stops us
    // differs from run to run (a trainer's sight depends on our row), so
    // there is no fixed list: walk east, talk or fight whoever stops us, and
    // at a wall find a row from which east goes on.
    size_t fights = 0;
    bool inside = false;
    //  On Route 203 only trainers start a talk; one with no battle is the
    //  Hiker, who meets us inside the Gate (whose fade may not read as a
    //  transition).
    bool met_hiker = false;
    auto stopped_by = [&](){
        if (talk_or_fight(stream, context, "Route 203 trainer " + std::to_string(fights + 1))){
            fights++;
        }else{
            route_status(stream, "No battle: that was the Hiker, inside the Oreburgh Gate.");
            met_hiker = true;
            inside = true;
        }
    };
    for (size_t leg = 0; leg < 40 && !inside; leg++){
        const WalkResult walk = walk_until(stream, context, run_from_wild_battle, {1, 0}, 10000ms, "east along Route 203");
        if (walk.stop == WalkStop::TRANSITION){
            inside = true;
            break;
        }
        if (walk.stop == WalkStop::LIMIT){
            continue;
        }
        if (walk.stop == WalkStop::DIALOG || dialogue_opens(stream, context, 1500ms)){
            stopped_by();
            continue;
        }
        // A wall. Find the row from which east goes on: north to the top wall
        // first (a step north there does not move us, so rows are counted
        // from it), then a row at a time south. At the route's end this is the
        // corner by the second sign, with the Gate's opening two rows down.
        walk_until(stream, context, run_from_wild_battle, {0, 1}, 6000ms, "north to the top wall");
        bool moved_on = false;
        for (size_t row = 1; row <= 10 && !moved_on; row++){
            step_south(stream, context);
            const WalkResult next = walk_until(stream, context, run_from_wild_battle, {1, 0}, 10000ms, "east along Route 203, row " + std::to_string(row) + " below the top");
            if (next.stop == WalkStop::TRANSITION){
                inside = true;
            }
            if (next.stop == WalkStop::DIALOG || (next.stop == WalkStop::BLOCKED && dialogue_opens(stream, context, 1500ms))){
                stopped_by();
            }
            //  Two still chunks are the least a wall takes; more is progress.
            moved_on = inside || next.stop != WalkStop::BLOCKED || next.walked > 1200ms;
        }
        if (!moved_on){
            fail_step(stream, "Route 203: walls east on every row down to ten below the top wall.");
        }
    }
    if (!inside){
        fail_step(stream, "Route 203: did not reach the Oreburgh Gate.");
    }
    route_status(stream, "Route 203: " + std::to_string(fights) + " trainer battle(s) on the way.");
    context.wait_for(1500ms);
    if (met_hiker){
        state.flags.insert("met_hiker");
    }
    return Pokemon::outcome_ok();
}

//  01.02: the Hiker, who gives HM Rock Smash, when the walk east has not met him.
void the_hiker(VideoStream& stream, ProControllerContext& context){
    // The Hiker steps up to us as we walk east.
    const WalkResult to_hiker = walk_until(stream, context, run_from_wild_battle, {1, 0}, 4000ms, "to the Hiker");
    if (to_hiker.stop != WalkStop::DIALOG && !dialogue_opens(stream, context, 2000ms)){
        fail_step(stream, "Oreburgh Gate: the Hiker did not stop us.");
    }
    finish_dialogue(stream, context, BUTTON_B, "Hiker");
}

//  01.02: the "Hidden Moves" card after the Hiker.
void hidden_moves_card(VideoStream& stream, ProControllerContext& context){
    // Then a "Hidden Moves" tutorial card, closed with B. It is no dialogue
    // box, and it can come up a moment after his last line.
    pbf_wait(context, 1500ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
    pbf_press_button(context, BUTTON_B, 80ms, 1000ms);
    context.wait_for_all_requests();
}

//  01.02: east through the Gate past Curtis and Diana, out to Oreburgh City.
void gate_east_to_oreburgh(VideoStream& stream, ProControllerContext& context){
    // East: Camper Curtis and Picnicker Diana see us; past them the exit's
    // white fade, and the man in Oreburgh stops us right outside.
    size_t fights = 0;
    bool city = false;
    size_t stuck = 0;
    for (size_t leg = 0; leg < 12 && !city; leg++){
        const WalkResult walk = walk_until(stream, context, run_from_wild_battle, {1, 0}, 10000ms, "east through the Oreburgh Gate");
        if (walk.stop == WalkStop::LIMIT || walk.stop == WalkStop::TRANSITION){
            continue;
        }
        if (walk.stop != WalkStop::DIALOG && !dialogue_opens(stream, context, 2000ms)){
            // A card or menu left up holds the screen still: B once, then retry.
            if (++stuck > 1){
                fail_step(stream, "Oreburgh Gate: a wall stopped us walking east.");
            }
            pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
            continue;
        }
        stuck = 0;
        if (fights >= 2){
            city = true;
            break;
        }
        fights += talk_or_fight(stream, context, "Oreburgh Gate trainer " + std::to_string(fights + 1)) ? 1 : 0;
    }
    if (!city){
        fail_step(stream, "Oreburgh Gate: did not reach Oreburgh City.");
    }
}

//  01.02: the man at the city's entrance and Barry, at the Pokemon Center.
void oreburgh_welcome(VideoStream& stream, ProControllerContext& context){
    // He walks us to the Pokemon Center between his lines, and Barry joins
    // there: more lines after each walk.
    finish_dialogue(stream, context, BUTTON_B, "Man at Oreburgh's entrance");
    for (size_t scene = 0; scene < 6 && dialogue_opens(stream, context, 6000ms); scene++){
        finish_dialogue(stream, context, BUTTON_B, "Oreburgh scene");
    }
    context.wait_for_all_requests();
}

//  01.06: Route 203 on its grid to row 18, below the girl.
Pokemon::Outcome route_203_to_row_18(StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep&){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    // On the grid (Maps/Route203.json, from the decomp,
    // its east stretch checked by hand): south from the Gate down the sand,
    // west along the sand's bottom row and on under the girl's row, which
    // runs straight to Jubilife as row 18. Trainers the map does not list
    // are fought where they stop us.
    RouteTrainers trainers{{}, [&](const std::string& who){
        talk_or_fight(stream, context, "Route 203: " + who);
    }};
    const std::vector<Pokemon::TilePos> at = walk_route(stream, context, run_from_wild_battle, route203_maps(), map_region("Route203", "gate_exit"), trainers, RouteWalkPlan{on_route203_west_line, "Route 203 to row 18", {}, route_plan_costs()});
    if (at.size() != 1){
        fail_step(stream, "Route 203: the walk to row 18 did not end on a known tile.");
    }
    return Pokemon::outcome_ok(Pokemon::tile_pose("Route203", at));
}

//  01.06: west along row 18 into Jubilife, counting tiles.
Pokemon::Outcome row_18_into_jubilife(StoryExecutor& executor, Pokemon::RunState& state, const Pokemon::StoryStep&){
    VideoStream& stream = executor.stream();
    ProControllerContext& context = executor.context();
    const std::vector<Pokemon::TilePos> at = Pokemon::tiles_of(state.pose);
    if (at.size() != 1){
        fail_step(stream, "Route 203: row 18's walk needs the one tile the walk to it ended on.");
    }
    // Row 18 has no wall before Jubilife, so count the tiles: from column
    // `x` it is x + 1 taps to Jubilife's first tile (by hand from column 31:
    // 32 taps, Jubilife's banner on the last one). 01.07 starts there.
    dpad_tiles(stream, context, DPAD_LEFT, (size_t)at[0].x + 1, "west along row 18 into Jubilife");
    context.wait_for_all_requests();
    if (!location_banner_showing(*stream.video().snapshot().frame)){
        fail_step(stream, "Route 203: no Jubilife banner after counting the tiles west.");
    }
    return Pokemon::outcome_ok();
}

}


void add_route203_events(StoryEvents& events){
    events.emplace("route_203_east_to_the_gate", route_203_east_to_the_gate);
    events.emplace("route_203_to_row_18", route_203_to_row_18);
    events.emplace("row_18_into_jubilife", row_18_into_jubilife);
    events.emplace("the_hiker", console_event(the_hiker));
    events.emplace("hidden_moves_card", console_event(hidden_moves_card));
    events.emplace("gate_east_to_oreburgh", console_event(gate_east_to_oreburgh));
    events.emplace("oreburgh_welcome", console_event(oreburgh_welcome));
}



}
}
}
