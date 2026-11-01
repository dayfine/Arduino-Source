/*  BDSP AutoStory - Events: Jubilife City (00.05, 01.01, 01.07)
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Jubilife beats of 00.05 (the Town Map, the president, the clowns' coupons,
 *  the Poketch), stepped through on the console on 2026-10-30, and
 *  segment 01's walks out of the Center to Barry and up to Rowan's scene.
 *
 */

#include <cmath>
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
    route_turn(stream, context, Direction::WEST);
    walk_for(stream, context, run_from_wild_battle, {-1, 0}, 200ms, "To the steps");
    const WalkResult up = walk_for(stream, context, run_from_wild_battle, {0, 1}, 480ms, "Up the steps");
    if (up.stop == WalkStop::DIALOG || up.stop == WalkStop::TRANSITION){
        fail_step(stream, std::string("Up the steps: ") + walk_stop_name(up.stop) + ".");
    }
}

//  The School's lamp, east of its door, from its doorsteps' row at night
//  (2026-10-30): at 0.65 to 0.69 of the screen across the door takes us in;
//  at 0.61 (right of it) and 0.75 (past it) its front stops us. A tile is
//  0.065 of the screen. The box leaves out the Center's lamp further east.
const double SCHOOL_LAMP_X_AT_DOOR = 0.67;
const ImageFloatBox SCHOOL_LAMP_BOX(0.55, 0.20, 0.30, 0.10);
const size_t SCHOOL_TILES_WEST_OF_CENTER = 11;

//  At night the lamp east of the School's door says how many tiles across
//  the door is; nothing by day.
std::optional<double> school_door_across(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    VideoSnapshot screen = stream.video().snapshot();
    if (!screen){
        return std::nullopt;
    }
    const std::optional<double> lamp = find_lit_lamp_x(*screen.frame, SCHOOL_LAMP_BOX);
    if (!lamp){
        stream.log("[Route] School's door: no lit lamp on screen.");
        return std::nullopt;
    }
    const double across = tiles_across_at(*lamp) - tiles_across_at(SCHOOL_LAMP_X_AT_DOOR);
    stream.log("[Route] School's door: the lamp at " + std::to_string(*lamp) + " of the screen, the door " +
        std::to_string(across) + " tiles across.");
    return across;
}

//  00.05: from the Center's doorstep into the Trainers' School. Measured on
//  the console (2026-10-30): a tap to face west and eleven one-tile holds put
//  us under the School's doorway; twelve is past it. A hold ends a part of a
//  tile off (back east, eleven stopped beside the Center's door), so if the
//  School's front stops us, nudge half a tile each way.
void into_the_school(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::WEST);
    for (size_t tiles = 0; tiles < SCHOOL_TILES_WEST_OF_CENTER; tiles++){
        hold_for(stream, context, Direction::WEST, 160ms, "West to the School's door");
    }
    for (size_t looks = 0; looks < 3; looks++){
        const std::optional<double> across = school_door_across(stream, context);
        //  The door took us from -0.19 and 0.19 but not -0.34 (r148b).
        if (!across || std::abs(*across) <= 0.3){
            break;
        }
        stream.log("[Route] Line up with the School's door by its lamp: " + std::to_string(*across) + " tiles across.");
        move_across(stream, context, *across);
        context.wait_for_all_requests();
    }
    if (door_north_by_scene(stream, context, "Trainers' School door")){
        return;
    }
    for (Direction side : {Direction::EAST, Direction::WEST, Direction::WEST}){
        nudge(stream, context, side, "Line up with the School's door");
        if (door_north_by_scene(stream, context, "Trainers' School door")){
            return;
        }
    }
    fail_step(stream, "The School's door did not take us from 11 tiles west of the Center's doorstep, or half a tile either side.");
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

//  00.05: the clown at the east end of the road between the Center and the
//  Mart. He may be what stopped the walk east (r155c, 2.8 s against 2.2):
//  ask ahead first. Else he stands a row south and a tile west: try that
//  row, then one more, facing west each time. His question's default
//  answer, Yes, is right; A takes his coupon.
void clown_by_the_mart(VideoStream& stream, ProControllerContext& context){
    DialogueRead read = talk_facing(stream, context, Direction::EAST, BUTTON_A, "Clown by the Poke Mart");
    for (size_t rows = 0; rows < 2 && !read.opened; rows++){
        hold_for(stream, context, Direction::SOUTH, 200ms, "A row south");
        route_turn(stream, context, Direction::WEST);
        read = talk_facing(stream, context, Direction::WEST, BUTTON_A, "Clown by the Poke Mart");
    }
    if (!read.opened || !read.closed){
        fail_step(stream, "Jubilife: the clown by the Poke Mart did not talk.");
    }
}

//  The Poketch Company's sign reads "...World..." or "...Company...".
bool is_company_sign(const DialogueRead& read){
    return read.text.find("World") != std::string::npos || read.text.find("Company") != std::string::npos;
}

//  Him north of us, or the sign.
bool ask_clown(VideoStream& stream, ProControllerContext& context, Direction direction){
    const DialogueRead read = talk_facing(stream, context, direction, BUTTON_A, "Clown by the Poketch Company");
    return read.opened && !is_company_sign(read);
}

//  00.05: the clown wanders the plaza behind the hedges, and a walk can pass
//  him by. Measured on the console (2026-10-30): from the lamp, the Poketch
//  Company's sign is six tiles east along the hedges; it reads facing north,
//  and the tile east of it is a gap into the plaza. A step into the gap, then
//  A every second or so, talked to him within seconds: he comes by. So tap
//  four tiles, then a tile at a time asking north (him, or the sign), and
//  wait in the gap. A turn tap the way we already face is a step (r161c), so
//  we face east between two asks north.
void clown_by_the_company(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::EAST);
    tap_tiles(stream, context, Direction::EAST, 4, "East along the hedges");
    bool talked = false;
    bool in_gap = false;
    for (size_t tiles = 0; tiles < 5 && !in_gap && !talked; tiles++){
        talked = ask_clown(stream, context, Direction::NORTH);
        if (talked){
            break;
        }
        route_turn(stream, context, Direction::EAST);
        const DialogueRead sign = read_sign(stream, context, Direction::NORTH, "the Poketch Company's sign");
        if (sign.opened && !is_company_sign(sign)){
            //  Him after all (or text OCR missed); his talk was closed with B: ask again.
            route_turn(stream, context, Direction::EAST);
            talked = ask_clown(stream, context, Direction::NORTH);
            if (talked){
                break;
            }
        }
        route_turn(stream, context, Direction::EAST);
        tap_tiles(stream, context, Direction::EAST, 1, "On along the hedges");
        if (sign.opened){
            //  East of the sign: north is the gap.
            hold_for(stream, context, Direction::NORTH, 200ms, "Into the gap beside the sign");
            in_gap = true;
        }
    }
    //  Up to about a minute and a half; west of the gap is the sign. East
    //  first: the step into the gap left us facing north.
    for (size_t round = 0; round < 18 && in_gap && !talked; round++){
        for (Direction direction : {Direction::EAST, Direction::NORTH, Direction::WEST}){
            talked = ask_clown(stream, context, direction);
            if (talked){
                break;
            }
        }
    }
    if (!talked){
        fail_step(stream, "Jubilife: the clown by the Poketch Company did not talk.");
    }
}

//  00.05: the president stands still a row or two north of the lamp's row,
//  a few tiles west: from a row up, west a tile at a time, asking ahead and
//  north. Turns only ever change the way we face, so none is a step. His
//  last line asks us to try the R Button; a help panel about the Poketch
//  follows, closed with B.
void poketch_from_the_president(VideoStream& stream, ProControllerContext& context){
    hold_for(stream, context, Direction::NORTH, 200ms, "A row up from the lamp");
    route_turn(stream, context, Direction::WEST);
    DialogueRead read;
    for (size_t tiles = 0; tiles < 8 && !read.opened; tiles++){
        read = talk_ahead(stream, context, BUTTON_B, "The Poketch Company president");
        if (read.opened){
            break;
        }
        route_turn(stream, context, Direction::NORTH);
        read = talk_ahead(stream, context, BUTTON_B, "The Poketch Company president");
        if (read.opened){
            break;
        }
        route_turn(stream, context, Direction::WEST);
        tap_tiles(stream, context, Direction::WEST, 1, "West along the road");
    }
    if (!read.opened || !read.closed){
        fail_step(stream, "Jubilife: the president did not give us the Poketch.");
    }
    stream.log("[Talk] The Poketch's help: B to close the panel.");
    pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
    context.wait_for_all_requests();
}

//  01.07: Rowan's and Dawn's lines after the tag battle. 01.08 starts here,
//  from a save at the top of the street.
void rowan_after_the_battle(VideoStream& stream, ProControllerContext& context){
    finish_dialogue(stream, context, BUTTON_B, "Rowan and Dawn after the battle");
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
    events.emplace("into_the_school", console_event(into_the_school));
    events.emplace("barry_town_map", console_event(barry_town_map));
    events.emplace("clown_by_the_mart", console_event(clown_by_the_mart));
    events.emplace("clown_by_the_company", console_event(clown_by_the_company));
    events.emplace("poketch_from_the_president", console_event(poketch_from_the_president));
    events.emplace("rowan_after_the_battle", console_event(rowan_after_the_battle));
    events.emplace("jubilife_center_to_street", console_event(jubilife_center_to_street));
    events.emplace("east_to_barry", console_event(east_to_barry));
    events.emplace("west_to_the_crossroads", console_event(west_to_the_crossroads));
    events.emplace("north_to_rowan", console_event(north_to_rowan));
}



}
}
}
