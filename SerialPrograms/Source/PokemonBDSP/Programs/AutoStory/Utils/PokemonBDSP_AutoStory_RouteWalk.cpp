/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <format>
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Navigation/Pokemon_RouteLog.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_Indoors.h"
#include "PokemonBDSP_AutoStory_OverlayLog.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

bool across(Direction direction){
    return direction == Direction::EAST || direction == Direction::WEST;
}

}

Milliseconds route_ms_per_tile(Direction direction){
    return across(direction) ? ROUTE_MS_PER_TILE_ACROSS : ROUTE_MS_PER_TILE_UP_DOWN;
}

JoystickPosition route_stick(Direction direction){
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


bool route_wall_readable(const StoryWorld& grid, const std::vector<TilePos>& after){
    return std::none_of(after.begin(), after.end(), [&](const TilePos& pos){
        return grid.tile(pos) == TileKind::TALL_GRASS;
    });
}

void route_status(VideoStream& stream, const std::string& text){
    stream.log("[Route] " + text, COLOR_BLUE);
    add_wrapped_log(stream.overlay(), text, COLOR_WHITE);
}

Milliseconds route_open_hold(Direction direction, size_t tiles){
    const Milliseconds step = route_ms_per_tile(direction);
    return step * tiles - step / 2;
}

void route_turn(VideoStream& stream, ProControllerContext& context, Direction direction){
    stream.log(std::string("[Route] Turning ") + direction_name(direction) + ".");
    pbf_move_left_joystick(context, route_stick(direction), ROUTE_TURN_TAP, 300ms);
    context.wait_for_all_requests();
}

std::optional<Direction> talk_to_adjacent_npc(
    VideoStream& stream, ProControllerContext& context,
    const std::vector<Direction>& directions,
    const std::string& label, Milliseconds wait
){
    for (Direction direction : directions){
        route_turn(stream, context, direction);
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_CYAN);
        if (wait_until(stream, context, wait, {{dialog}}) == 0){
            stream.log("[Route] " + label + ": answered from the " + direction_name(direction) + ".");
            return direction;
        }
    }
    stream.log("[Route] " + label + ": nobody answered next to us.");
    return std::nullopt;
}

bool talk_with_nudges(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    Direction facing, const std::string& label, Milliseconds wait,
    const std::function<bool(const ImageViewRGB32& before, const ImageViewRGB32& after)>& accept
){
    const bool vertical = facing == Direction::NORTH || facing == Direction::SOUTH;
    const Direction minus = vertical ? Direction::WEST : Direction::NORTH;
    const Direction plus = vertical ? Direction::EAST : Direction::SOUTH;
    int at = 0;
    for (int offset : {0, -1, 1, -2, 2}){
        const Direction side = offset < at ? minus : plus;
        for (; at != offset; at += offset < at ? -1 : 1){
            pbf_move_left_joystick(context, route_stick(side), ROUTE_NUDGE_TAP, 300ms);
        }
        if (offset != 0){
            route_turn(stream, context, facing);
            //  A tap in a cave or in grass can start a wild battle, after
            //  which we are not where the nudges think: let the caller line up again.
            if (run_from_late_encounter(stream, context, escape, label + ": nudge")){
                return false;
            }
        }
        context.wait_for_all_requests();
        const VideoSnapshot before = stream.video().snapshot();
        pbf_press_button(context, BUTTON_A, 80ms, 0ms);
        context.wait_for_all_requests();
        ShortDialogWatcher dialog(COLOR_CYAN);
        if (wait_until(stream, context, wait, {{dialog}}) == 0){
            const VideoSnapshot after = stream.video().snapshot();
            if (!accept || (before && after && accept(*before.frame, *after.frame))){
                stream.log("[Route] " + label + ": answered " + std::to_string(offset) + " nudge(s) from where we stood.");
                return true;
            }
            stream.log("[Route] " + label + ": something else answered " + std::to_string(offset) + " nudge(s) from where we stood; closing it.");
            for (size_t press = 0; press < 3; press++){
                pbf_press_button(context, BUTTON_B, 80ms, 700ms);
            }
            context.wait_for_all_requests();
        }
    }
    stream.log("[Route] " + label + ": nobody answered within two nudges either side.");
    return false;
}


void finish_dialogue(
    VideoStream& stream, ProControllerContext& context,
    Button button, const std::string& label, Milliseconds timeout
){
    //  No line counts: talks differ in length (r115a: 8 A left the Poke Mart
    //  lady's Potions talk open). The box blinks off between lines and during
    //  "obtained" jingles, so only a stable absence ends the talk.
    const WallClock none = WallClock::max();
    ShortDialogDetector dialog;
    OverworldDetector overworld;
    const WallClock deadline = current_time() + timeout;
    WallClock quiet_since = none;
    WallClock unknown_since = none;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const WallClock now = current_time();
        const bool talking = frame && dialog.detect(frame);
        if (frame && !talking && overworld.detect(frame)){
            unknown_since = none;
            if (quiet_since == none){
                quiet_since = now;
            }else if (now - quiet_since >= 1500ms){
                return;
            }
            context.wait_for(200ms);
            continue;
        }
        quiet_since = none;
        //  Press only on a box (an A on the overworld talks again), or on a
        //  screen that is neither for 3 s.
        if (talking || (unknown_since != none && now - unknown_since >= 3000ms)){
            unknown_since = none;
            pbf_press_button(context, button, 80ms, 1200ms);
            continue;
        }
        if (unknown_since == none){
            unknown_since = now;
        }
        context.wait_for(200ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": the dialogue did not end within " +
        std::to_string(std::chrono::duration_cast<std::chrono::seconds>(timeout).count()) + " s.", stream);
}

void enter_door_north(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label){
    //  Sideways steps to try after a blocked walk: east, then back west and
    //  one more.
    const Direction steps[] = {Direction::EAST, Direction::WEST, Direction::WEST};
    for (size_t attempt = 0; attempt <= 3; attempt++){
        const WalkResult walk = walk_until(stream, context, escape, {0, 1}, 2500ms, label);
        if (walk.stop == WalkStop::TRANSITION){
            context.wait_for(1500ms);
            return;
        }
        if (attempt == 3){
            break;
        }
        route_status(stream, label + ": the door frame stopped us; one tile " +
            (steps[attempt] == Direction::EAST ? "east." : "west."));
        route_turn(stream, context, steps[attempt]);
        const double x = steps[attempt] == Direction::EAST ? 1 : -1;
        pbf_move_left_joystick(context, {x, 0}, route_open_hold(steps[attempt], 1), 200ms);
        context.wait_for_all_requests();
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": could not go through the door.", stream);
}

bool door_fade_look(DoorFadeWatch& watch, double brightness, bool room){
    if (brightness < DOOR_FADE_DARK){
        watch.saw_dark = true;
    }
    return watch.saw_dark && !room && brightness >= DOOR_OUTSIDE_BRIGHT;
}

bool door_walk_stops(DoorFadeWatch& watch, double brightness, bool room){
    //  Both are asked: the fade watch keeps what it has seen.
    const bool faded = door_fade_look(watch, brightness, room);
    return faded || !room;
}

namespace{

//  Three looks half a second apart, none a room: outside, whatever the fade
//  did. A look of a lit Center can miss its black edge (r259 at 800 ms),
//  three in a row standing still do not.
bool outside_for_three_looks(VideoStream& stream, ProControllerContext& context, const std::string& label){
    for (size_t look = 0; look < 3; look++){
        context.wait_for_all_requests();
        VideoSnapshot now = stream.video().snapshot();
        const bool room = !now || indoors(*now.frame);
        stream.log("[Route] " + label + ": standing look " + std::to_string(look + 1) + ": " + (room ? "a room" : "not a room") + ".");
        if (room){
            return false;
        }
        pbf_wait(context, 500ms);
    }
    return true;
}

}

void leave_door_south(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label){
    //  A last line of talk may still be up, and it would stop the walk.
    pbf_mash_button(context, BUTTON_B, 2000ms);
    context.wait_for_all_requests();
    //  Stop the walk at the fade or at the first look that is not a room: a
    //  stick held on walks on outside (r124a ten rows south of the Mart's
    //  door, r257 into a house, r261 three rows). Standing looks then confirm.
    DoorFadeWatch watch;
    auto out_of_door = [&](const ImageViewRGB32& screen){
        const double brightness = image_stats(screen).average.sum();
        const bool room = indoors(screen);
        const bool out = door_walk_stops(watch, brightness, room);
        stream.log(std::format("[Route] {}: look: brightness {:.0f}, {}, fade {}.", label, brightness,
            room ? "a room" : "not a room", watch.saw_dark ? "seen" : "not seen"));
        return out;
    };
    bool out = false;
    for (size_t tries = 0; tries < 5 && !out; tries++){
        const WalkResult walk = walk_until(stream, context, escape, {0, -1}, 6000ms, label, out_of_door);
        if (walk.stop == WalkStop::DIALOG){
            pbf_mash_button(context, BUTTON_B, 2000ms);
        }
        //  Let the fade-in finish, then look again standing still.
        pbf_wait(context, 1500ms);
        out = walk.stop == WalkStop::TRANSITION || outside_for_three_looks(stream, context, label);
        if (!out){
            stream.log("[Route] " + label + ": still indoors; walking on.");
        }
    }
    if (!out){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": did not get back out.", stream);
    }
    //  Let the fade-in finish.
    pbf_wait(context, 1500ms);
}



}
}
}
