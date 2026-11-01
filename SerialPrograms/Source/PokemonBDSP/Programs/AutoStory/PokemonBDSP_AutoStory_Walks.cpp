/*  BDSP AutoStory - Walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Walks measured on the console that the planner does not make: straight
 *  holds and D-pad taps, walks until something stops us, and the ways in
 *  and out of doors.
 *
 */

#include <cstdlib>
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Talks.h"
#include "PokemonBDSP_AutoStory_WildBattle.h"
#include "PokemonBDSP_AutoStory_Walks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using Pokemon::Direction;


std::pair<double, double> dpad_of(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return {0, 1};
    case Direction::EAST:
        return {1, 0};
    case Direction::SOUTH:
        return {0, -1};
    case Direction::WEST:
        return {-1, 0};
    }
    return {0, 0};
}

void walk_until_stopped(VideoStream& stream, ProControllerContext& context, Direction direction, size_t tiles, const std::string& label){
    const bool across = direction == Direction::EAST || direction == Direction::WEST;
    const auto [x, y] = dpad_of(direction);
    //  Time for every tile, and for the wall to read as still.
    const Milliseconds limit = ROUTE_START_DELAY + 800ms +
        (int64_t)tiles * (across ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN);
    //  Turn first: the first press of a new way only turns us, the screen
    //  holds still meanwhile, and that read as a wall two tiles into the
    //  walk west to the Lab's sign (r132a).
    route_turn(stream, context, direction);
    const WalkResult walked = walk_until(stream, context, run_from_wild_battle, {x, y}, limit, label);
    stream.log("[Walk] " + label + ": stopped on " + walk_stop_name(walked.stop) + ".");
    if (walked.stop == WalkStop::TRANSITION || walked.stop == WalkStop::DIALOG){
        fail_step(stream, label + ": " + walk_stop_name(walked.stop) + " instead of a stop.");
    }
    //  The leg was measured to end against a wall, so holding toward it
    //  for every tile's time leaves us there whatever the screen shows:
    //  people walking or a TV screen nearby kept it changing and the stop
    //  went unseen (r162a, r167a, r171a). Seeing the stop only ends the
    //  hold early; a wrong place is caught by the next gate (a sign, a
    //  door, someone who answers).
    if (walked.stop == WalkStop::LIMIT){
        stream.log("[Walk] " + label + ": held for " + std::to_string(tiles) +
            " tiles' time without seeing the stop; taking the wall as reached.");
    }
}

void hold_for(VideoStream& stream, ProControllerContext& context, Direction direction, Milliseconds hold, const std::string& label){
    const auto [x, y] = dpad_of(direction);
    const WalkResult walk = walk_for(stream, context, run_from_wild_battle, {x, y}, hold, label);
    if (walk.stop == WalkStop::DIALOG || walk.stop == WalkStop::TRANSITION){
        fail_step(stream, label + ": " + walk_stop_name(walk.stop) + ".");
    }
}

void tap_tiles(VideoStream& stream, ProControllerContext& context, Direction direction, size_t tiles, const std::string& label){
    const auto [x, y] = dpad_of(direction);
    stream.log("[Walk] " + label + ": " + std::to_string(tiles) + " taps.");
    for (size_t i = 0; i < tiles; i++){
        pbf_move_left_joystick(context, {x, y}, 160ms, 400ms);
    }
    pbf_wait(context, 800ms);
    context.wait_for_all_requests();
}

void nudge(VideoStream& stream, ProControllerContext& context, Direction direction, const std::string& label){
    route_turn(stream, context, direction);
    walk_for(stream, context, run_from_wild_battle, {direction == Direction::EAST ? 1.0 : -1.0, 0.0}, 80ms, label);
}

bool door_north_by_scene(VideoStream& stream, ProControllerContext& context, const std::string& label){
    context.wait_for_all_requests();
    VideoSnapshot before = stream.video().snapshot();
    const WalkResult up = walk_until(stream, context, run_from_wild_battle, {0, 1}, 1200ms, label);
    context.wait_for(1500ms);
    if (up.stop == WalkStop::TRANSITION){
        return true;
    }
    //  The School's fade is not always black enough to read as one: r146b
    //  went in (changes 156, 261) and read "limit". A whole new scene is
    //  through, as leave_door_south() counts it.
    VideoSnapshot after = stream.video().snapshot();
    const bool through = before && after && walk_screen_change(*before.frame, *after.frame) > WALK_SCENE_CHANGE;
    if (through){
        stream.log("[Route] " + label + ": a new scene; through the door.");
    }
    return through;
}

//  One tile south, for a player facing another way.
void step_south(VideoStream& stream, ProControllerContext& context){
    route_turn(stream, context, Direction::SOUTH);
    pbf_move_left_joystick(context, {0, -1}, route_open_hold(Direction::SOUTH, 1), 200ms);
    context.wait_for_all_requests();
}

//  Holds the stick toward `dir` for `tiles` tiles, turning first.
void hold_tiles(VideoStream& stream, ProControllerContext& context, Direction dir, size_t tiles){
    route_turn(stream, context, dir);
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
    pbf_move_left_joystick(context, {x, y}, route_open_hold(dir, tiles), 200ms);
    context.wait_for_all_requests();
}

WalkResult walk_dir(
    VideoStream& stream, ProControllerContext& context, Direction dir, Milliseconds limit, const std::string& label,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived
){
    const double x = dir == Direction::EAST ? 1 : dir == Direction::WEST ? -1 : 0;
    const double y = dir == Direction::NORTH ? 1 : dir == Direction::SOUTH ? -1 : 0;
    return walk_until(stream, context, run_from_wild_battle, {x, y}, limit, label, arrived);
}

//  `taps` D-pad taps toward `dpad`, settling between them. On the D-pad the
//  player moves a whole tile per tap (measured above), so counted legs do
//  not drift the way stick holds do. The first tap toward a new way only
//  turns the player, so turn first.
void dpad_tiles(VideoStream& stream, ProControllerContext& context, DpadPosition dpad, size_t taps, const std::string& label){
    stream.log("[Walk] " + label + ": " + std::to_string(taps) + " D-pad taps.");
    pbf_press_dpad(context, dpad, 160ms, 700ms);    // turn
    // Count only taps that moved the view: one after the turn was lost and
    // the leg ended a tile short (r215d). A tile's scroll changes the view by
    // about 100; standing still, under 15. A tap that does not move is someone
    // in the way: people walk about these streets (r220g: blocked on tap 9
    // for 12 s while a passer-by stood in our row). Wait for them to move on,
    // up to 30 s, before giving up.
    size_t moved = 0;
    const auto deadline = current_time() + std::chrono::milliseconds(taps * 1000 + 30000);
    while (moved < taps && current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot before = stream.video().snapshot();
        pbf_press_dpad(context, dpad, 160ms, 700ms);
        context.wait_for_all_requests();
        const VideoSnapshot after = stream.video().snapshot();
        const double change = before && after ? walk_screen_change(*before.frame, *after.frame) : 100;
        if (change > 30){
            moved++;
            continue;
        }
        stream.log("[Walk] " + label + ": tap " + std::to_string(moved + 1) + " did not move (view change " + std::to_string(change) + "); someone in the way? Waiting.");
        if (dialogue_opens(stream, context, 300ms)){
            talk_or_fight(stream, context, label + ": someone in the way");
        }
        context.wait_for(1500ms);
    }
    if (moved < taps){
        fail_step(stream, label + ": only " + std::to_string(moved) + " of " + std::to_string(taps) + " D-pad taps moved us.");
    }
}

//  Through a door straight north of us: tap up to face it, hold up into it,
//  and wait for its dark fade (by hand: brightness 7, about 1 s after).
bool dpad_through_door_north(VideoStream& stream, ProControllerContext& context, const std::string& label){
    pbf_press_dpad(context, DPAD_UP, 160ms, 300ms);
    pbf_press_dpad(context, DPAD_UP, 300ms, 0ms);
    context.wait_for_all_requests();
    const auto start = current_time();
    while (current_time() - start < 5000ms){
        context.wait_for(100ms);
        const VideoSnapshot frame = stream.video().snapshot();
        if (frame && mean_brightness(*frame.frame) < 25){
            stream.log("[Route] " + label + ": the screen went dark; through the door.");
            context.wait_for(2000ms);
            return true;
        }
    }
    stream.log("[Route] " + label + ": no dark screen after the D-pad step north.");
    return false;
}

//  Out of a building whose door is straight south of us, a D-pad tile at a
//  time, until the location banner shows: it names the map we step into
//  (e.g. "Oreburgh City") for a few seconds, and never shows inside. Ends in
//  the doorway, facing south. Out of the Oreburgh Gym from in front of Roark
//  (by hand, 2026-10-31): 18 taps inside, 2 taps of black fade, the banner on
//  the 21st. The door's fade alone was missed between steps (r219g walked on
//  south to the rocks below the gym).
bool dpad_out_of_building(VideoStream& stream, ProControllerContext& context, size_t max_taps, const std::string& label){
    pbf_press_dpad(context, DPAD_DOWN, 160ms, 700ms);    // turn
    for (size_t tap = 0; tap < max_taps; tap++){
        pbf_press_dpad(context, DPAD_DOWN, 160ms, 900ms);
        context.wait_for_all_requests();
        const VideoSnapshot frame = stream.video().snapshot();
        if (frame && location_banner_showing(*frame.frame)){
            stream.log("[Walk] " + label + ": the location banner shows after " + std::to_string(tap + 1) + " taps; outside.");
            context.wait_for(1000ms);
            return true;
        }
        if (dialogue_opens(stream, context, 100ms)){
            finish_dialogue(stream, context, BUTTON_B, label + ": someone in the way");
        }
    }
    return false;
}

//  For a player inside a Pokemon Center after a whiteout, at the counter:
//  out through the door. The door is straight south of the nurse in most
//  Centers; in Oreburgh's it was two tiles east of where we stood (by hand).
void walk_out_of_center(VideoStream& stream, ProControllerContext& context, const std::string& label){
    finish_dialogue(stream, context, BUTTON_B, label + ": the nurse");
    const int shifts[] = {0, 2, -4};
    for (int shift : shifts){
        if (shift != 0){
            hold_tiles(stream, context, shift > 0 ? Direction::EAST : Direction::WEST, (size_t)std::abs(shift));
        }
        // A tile at a time, looking after each, so we stop on the doorstep:
        // walk_until() looks only between 400 ms holds, and r205d held on
        // through the fade and stepped a tile further south.
        bool out = false;
        for (size_t tile = 0; tile < 10 && !out; tile++){
            // D-pad taps: whole tiles (the first only turns us from the nurse).
            pbf_press_dpad(context, DPAD_DOWN, 160ms, 0ms);
            context.wait_for(1500ms);
            if (dialogue_opens(stream, context, 100ms)){
                finish_dialogue(stream, context, BUTTON_B, label + ": someone in the Center");
            }
            const VideoSnapshot frame = stream.video().snapshot();
            size_t percent = 100;
            if (frame){
                center_floor_on_screen(*frame.frame, &percent);
            }
            out = percent < 30;
        }
        if (out){
            // "No floor" also reads true on the door's black fade, and the game
            // takes no input until it ends: the first D-pad taps after it were
            // lost and the gym's door ended a tile away (r215d, r216d; a shot
            // per press by hand showed the black frame). Wait for daylight.
            const auto start = current_time();
            while (current_time() - start < 6000ms){
                context.wait_for(100ms);
                const VideoSnapshot frame = stream.video().snapshot();
                if (frame && mean_brightness(*frame.frame) > 40){
                    break;
                }
            }
            context.wait_for(1000ms);
            return;
        }
    }
    fail_step(stream, label + ": could not find the Pokemon Center's door.");
}

//  West until a transition, by the user's rule for a wall in the way: walk
//  north to the top wall, which pins the row, then try the rows south of it
//  one at a time. Trainers who stop us are fought. Returns at the first
//  transition (a building, a cave, a gate), or when `arrived` matches.
void west_until_transition(
    VideoStream& stream, ProControllerContext& context, const std::string& label, size_t max_legs,
    const std::function<bool(const ImageViewRGB32& screen)>& arrived
){
    for (size_t leg = 0; leg < max_legs; leg++){
        const WalkResult west = walk_dir(stream, context, Direction::WEST, 20000ms, label, arrived);
        if (west.stop == WalkStop::ARRIVED){
            return;
        }
        if (west.stop == WalkStop::TRANSITION){
            context.wait_for(2000ms);
            return;
        }
        if (west.stop == WalkStop::DIALOG){
            talk_or_fight(stream, context, label + ": someone in the way");
            continue;
        }
        if (west.stop != WalkStop::BLOCKED){
            continue;
        }
        walk_dir(stream, context, Direction::NORTH, 4000ms, label + ": north to the top wall");
        bool moved = false;
        for (size_t row = 0; row < 10 && !moved; row++){
            hold_tiles(stream, context, Direction::SOUTH, 1);
            const WalkResult probe = walk_dir(stream, context, Direction::WEST, 1500ms, label + ": west on the next row", arrived);
            if (probe.stop == WalkStop::ARRIVED){
                return;
            }
            moved = probe.stop != WalkStop::BLOCKED || probe.walked > 1200ms;
            if (probe.stop == WalkStop::TRANSITION){
                context.wait_for(2000ms);
                return;
            }
            if (probe.stop == WalkStop::DIALOG){
                talk_or_fight(stream, context, label + ": someone in the way");
            }
        }
        if (!moved){
            fail_step(stream, label + ": no row below the top wall leads west.");
        }
    }
    fail_step(stream, label + ": no transition after " + std::to_string(max_legs) + " legs.");
}



}
}
}
