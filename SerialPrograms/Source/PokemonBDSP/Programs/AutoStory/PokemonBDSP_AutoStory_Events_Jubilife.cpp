/*  BDSP AutoStory - Events: Jubilife City (00.05, 01.01, 01.07)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Jubilife beats of 00.05 the story table cannot plan (up the steps,
 *  Barry's Town Map, the talk to a wandering clown), and segment 01's walks
 *  out of the Center to Barry and up to Rowan's scene.
 *
 */

#include <cmath>
#include <format>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Center.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_Walks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Events.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  00.05: from the save below Jubilife's steps, against the wall east of
//  them: a tile west (the first press only turns), then up the steps.
void up_the_steps(VideoStream& stream, ProControllerContext& context){
    //  D-pad taps, a tile each: the stick holds this was (200 ms west, 480 ms
    //  north) left us between tiles, and every leg after it a column off
    //  (r57 to r61). By hand: a tap west and taps north from the save land
    //  on the map's tiles (between the lamps at x 43 and 49 after six).
    route_turn(stream, context, Direction::WEST);
    walk_tiles_dpad(stream, context, run_from_wild_battle, {-1, 0}, 1, "To the steps");
    route_turn(stream, context, Direction::NORTH);
    const WalkResult up = walk_tiles_dpad(stream, context, run_from_wild_battle, {0, 1}, 2, "Up the steps");
    if (up.stop == WalkStop::DIALOG || up.stop == WalkStop::TRANSITION){
        fail_step(stream, std::string("Up the steps: ") + walk_stop_name(up.stop) + ".");
    }
}

//  00.05: Barry stands at the blackboard a tile west of the door's column,
//  and that column is clear to the back wall: north to it, face west, his
//  scene to "You obtained a Town Map!" (A closes it). A tile west by a hold
//  went two in r149a and the boy at the table stopped us.
void barry_town_map(VideoStream& stream, ProControllerContext& context){
    for (size_t attempt = 0; attempt < 2; attempt++){
        const DialogueRead read = talk_facing(stream, context, Direction::WEST, BUTTON_A, "Barry");
        if (read.opened && read.closed){
            return;
        }
        walk_until_stopped(stream, context, Direction::NORTH, 4, "North to the back wall");
    }
    fail_step(stream, "Trainers' School: Barry did not answer from beside the back wall.");
}

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


//  01.01 from the save at the Jubilife Center's PC: healed at the nurse,
//  out of the door and through the plaza to the street east to Route 203.
void jubilife_heal_and_out_to_street(VideoStream& stream, ProControllerContext& context){
    // The save stands us at the PC, right of the counter, facing it; west
    // is the counter's end. Two tiles south clears it, then three west is
    // the floor emblem's column, with the nurse north and the door south.
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(Direction::SOUTH, 2), 200ms);
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Direction::WEST, 3), 200ms);
    if (walk_to_talk(stream, context, run_from_wild_battle, {0, 1}, 3000ms, "to the nurse").stop != WalkStop::DIALOG){
        fail_step(stream, "Jubilife Pokemon Center: the nurse did not answer.");
    }
    // A throughout: Yes to "rest your Pokemon?" (B would answer No).
    finish_dialogue(stream, context, BUTTON_A, "Nurse");
    // Healing shows no text box for a few seconds, which reads as the end;
    // her "Thank you for waiting" lines follow it.
    if (dialogue_opens(stream, context, 6000ms)){
        finish_dialogue(stream, context, BUTTON_B, "Nurse, after healing");
    }
    // South through the door until a girl standing below it stops us. The
    // door's fade may or may not read as a transition.
    WalkResult out{WalkStop::LIMIT, 0ms, 0};
    for (size_t walk = 0; walk < 3 && out.stop != WalkStop::BLOCKED; walk++){
        out = walk_until(stream, context, run_from_wild_battle, {0, -1}, 8000ms, "out of the Pokemon Center");
        if (out.stop == WalkStop::DIALOG || out.stop == WalkStop::ARRIVED){
            break;
        }
    }
    if (out.stop != WalkStop::BLOCKED){
        fail_step(stream, std::string("Jubilife Pokemon Center: walking out ended with ") + walk_stop_name(out.stop) + ", not at the girls below the door.");
    }

    // Jubilife's plaza has too few walls the grid gets right to plan on (a
    // leg to a wall the map shows walked on 19 tiles). These are the holds
    // that worked on the console: west onto the main road, north to the
    // street east to Route 203, and back a couple of tiles.
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, 720ms, 300ms);
    route_turn(stream, context, Direction::NORTH);
    pbf_move_left_joystick(context, {0, 1}, 3960ms, 300ms);
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, 600ms, 300ms);
    context.wait_for_all_requests();
}

//  01.01: from the Center's PC, the nurse and out, then the holds to the street east to Route 203;
//  nothing when 00.05 has left us outdoors on that street.
void jubilife_center_to_street(VideoStream& stream, ProControllerContext& context){
    // From 00.05g we are already outdoors on the street east to Route 203,
    // healed on arriving in Jubilife: straight to the walk east.
    bool outdoors = false;
    context.wait_for_all_requests();
    if (VideoSnapshot screen = stream.video().snapshot()){
        outdoors = !indoors(*screen.frame);
    }
    if (outdoors){
        stream.log("[Route] 01.01: outdoors; skipping the Pokemon Center.");
    }else{
        jubilife_heal_and_out_to_street(stream, context);
    }
}

//  01.01: east along the street until Barry stops us at the entrance to Route 203.
void east_to_barry(VideoStream& stream, ProControllerContext& context){
    // East along the street. The opening to the route is a few rows high;
    // Barry stops us a little way in. A wall means we are a row off it: try
    // rows to the south, then to the north.
    static const int ROW_CHANGES[] = {0, 1, 1, -3, -1};
    bool barry = false;
    for (int change : ROW_CHANGES){
        if (barry){
            break;
        }
        for (int i = 0; i < std::abs(change); i++){
            const Direction dir = change > 0 ? Direction::SOUTH : Direction::NORTH;
            route_turn(stream, context, dir);
            pbf_move_left_joystick(context, {0, change > 0 ? -1.0 : 1.0}, route_open_hold(dir, 1), 200ms);
        }
        context.wait_for_all_requests();
        WalkResult walk{WalkStop::LIMIT, 0ms, 0};
        for (size_t stretch = 0; stretch < 3 && walk.stop == WalkStop::LIMIT; stretch++){
            walk = walk_until(stream, context, run_from_wild_battle, {1, 0}, 9000ms, "east to Route 203");
        }
        barry = walk.stop == WalkStop::DIALOG;
    }
    if (!barry){
        fail_step(stream, "Route 203: Barry did not stop us at the entrance.");
    }
}

//  01.07: west along Jubilife's long road to the crossroads.
void west_to_the_crossroads(VideoStream& stream, ProControllerContext& context){
    // Route 203's row 18 carries on as Jubilife's long road. From the first
    // tile the crossroads with the street north to Route 204 is 14 tiles
    // west; the street is about 4 tiles wide, so a stick hold does (by hand,
    // 2026-10-31: 2 x 1120 ms landed mid-street; 7 D-pad taps stopped below
    // a building, and 18 went on to the road down to Route 202, r230b).
    route_turn(stream, context, Direction::WEST);
    pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Direction::WEST, 14), 300ms);
    context.wait_for_all_requests();
}

//  01.07: north up the street until Rowan's scene starts.
void north_to_rowan(VideoStream& stream, ProControllerContext& context){
    // North up the street: Rowan's scene starts a few tiles up (by hand, a
    // 6 s walk). It opens with a cutscene, not a dialog box, so the walk
    // reads as stopped; wait for his first line. The green-haired man walks
    // about the crossroads and may stand in our way: wait and walk again.
    bool scene = false;
    for (size_t tries = 0; tries < 3 && !scene; tries++){
        const WalkResult up = walk_dir(stream, context, Direction::NORTH, 6000ms, "north up the street to Route 204");
        scene = up.stop == WalkStop::DIALOG || dialogue_opens(stream, context, 8000ms);
        if (!scene){
            pbf_wait(context, 2500ms);
        }
    }
    if (!scene){
        fail_step(stream, "Jubilife: Rowan's scene did not start up the street north.");
    }
}

}


void add_jubilife_events(StoryEvents& events){
    events.emplace("up_the_steps", console_event(up_the_steps));
    events.emplace("barry_town_map", console_event(barry_town_map));
    events.emplace("talk_to_wanderer", talk_to_wanderer);
    events.emplace("jubilife_center_to_street", console_event(jubilife_center_to_street));
    events.emplace("east_to_barry", console_event(east_to_barry));
    events.emplace("west_to_the_crossroads", console_event(west_to_the_crossroads));
    events.emplace("north_to_rowan", console_event(north_to_rowan));
}



}
}
}
