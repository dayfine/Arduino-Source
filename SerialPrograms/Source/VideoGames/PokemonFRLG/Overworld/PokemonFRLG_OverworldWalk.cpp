/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_BattleDialogs.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  Not yet calibrated on the console.
constexpr Milliseconds TILE_TIME = 267ms;       //  16 frames at 60 fps, measured exact over 15 tiles
constexpr Milliseconds SURF_TILE_TIME = 133ms;  //  the game moves a surfing player at running speed
constexpr Milliseconds TURN_TAP = 40ms;         //  turns a player facing another way, without a step
constexpr Milliseconds TURN_SETTLE = 300ms;
constexpr Milliseconds FACE_HOLD = 40ms;        //  short enough to turn without stepping
constexpr Milliseconds FACE_SETTLE = 300ms;     //  the turn animation, before A reaches what is ahead
constexpr Milliseconds STEP_RELEASE = 100ms;
constexpr Milliseconds WARP_FADE_TIMEOUT = 3000ms;
constexpr Milliseconds WARP_ARRIVAL_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_PROMPT_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_ANIMATION = 3000ms;
//  After a warp's fade: leaving by a door the game walks the player a tile out
//  on its own, and a leg started before that ends loses tiles (on the console a
//  600ms wait cost 2 tiles).
constexpr Milliseconds AFTER_WARP = 1500ms;


DpadPosition dpad(MapDirection direction){
    switch (direction){
    case MapDirection::NORTH:
        return DPAD_UP;
    case MapDirection::EAST:
        return DPAD_RIGHT;
    case MapDirection::SOUTH:
        return DPAD_DOWN;
    case MapDirection::WEST:
        return DPAD_LEFT;
    }
    return DPAD_NONE;
}

void wait_out_warp(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    BlackScreenWatcher fade(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, WARP_FADE_TIMEOUT, {fade}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no fade after walking onto the warp to " + step.end.map + ".",
            console
        );
    }
    BlackScreenOverWatcher arrival(COLOR_RED);
    if (wait_until(console, context, WARP_ARRIVAL_TIMEOUT, {arrival}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): the screen stayed black after the warp to " + step.end.map + ".",
            console
        );
    }
}

//  A wild battle or a message (e.g. a Repel wearing off) stops the player
//  part-way through a leg, after which the route no longer matches. Checked
//  after each step so the walk stops where it went wrong.
void check_not_interrupted(ConsoleHandle& console, ProControllerContext& context, const RouteStep& step){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    if (!screen){
        return;
    }
    const bool dialog = WhiteDialogDetector(COLOR_RED).detect(screen);
    const bool battle = BattleDialogDetector(COLOR_RED).detect(screen) || BattleMenuDetector(COLOR_RED).detect(screen);
    if (dialog || battle){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            std::string("walk_overworld_route(): ") + (battle ? "a battle" : "a message") + " interrupted: " + to_string(step),
            console
        );
    }
}

void start_surf(ConsoleHandle& console, ProControllerContext& context, MapDirection direction, bool facing_water){
    //  "The water is a deep blue... Would you like to SURF?", with YES selected.
    //  An A pressed while the player is still turning is lost.
    if (!facing_water){
        pbf_press_dpad(context, dpad(direction), FACE_HOLD, FACE_SETTLE);
    }
    pbf_press_button(context, BUTTON_A, 160ms, 0ms);
    SelectionDialogWatcher prompt(COLOR_RED);
    context.wait_for_all_requests();
    if (wait_until(console, context, SURF_PROMPT_TIMEOUT, {prompt}) < 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "walk_overworld_route(): no Surf prompt facing the water.",
            console
        );
    }
    pbf_press_button(context, BUTTON_A, 160ms, 1000ms);

    //  "<Pokemon> used SURF!" waits for A, then the player jumps onto the water.
    for (int c = 0; c < 5; c++){
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        if (!screen || !WhiteDialogDetector(COLOR_RED).detect(screen)){
            break;
        }
        pbf_press_button(context, BUTTON_A, 160ms, 1000ms);
    }
    pbf_wait(context, SURF_ANIMATION);
}

}



void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route,
    std::optional<MapDirection> facing
){
    //  Each leg starts with a turn tap unless it goes the way the player faces.
    //  If the first facing isn't known it taps anyway, which takes an extra
    //  step if the player already faced that way.
    for (const RouteStep& step : route){
        console.log("Route: " + to_string(step));
        switch (step.action){
        case RouteAction::WALK:{
            //  An open leg is released half a tile before the last step would
            //  end: the step in progress completes and no further step starts.
            //  A leg to a wall is held two tiles longer, since the wall stops
            //  the player on its tile however long the stick is held.
            //  Turning first, with a tap, makes the hold start from a standstill
            //  facing the right way: holding through a turn lost a tile on the
            //  console, while holds from a standstill were exact over 15 tiles.
            if (facing != step.direction){
                pbf_press_dpad(context, dpad(step.direction), TURN_TAP, TURN_SETTLE);
            }
            const Milliseconds tile = step.surfing ? SURF_TILE_TIME : TILE_TIME;
            const Milliseconds hold = step.to_wall
                ? tile * (step.tiles + 2)
                : tile * (step.tiles - 1) + tile / 2;
            pbf_press_dpad(context, dpad(step.direction), hold, STEP_RELEASE);
            if (step.warps){
                wait_out_warp(console, context, step);
                pbf_wait(context, AFTER_WARP);
            }
            break;
        }
        case RouteAction::SURF:
            start_surf(console, context, step.direction, facing == step.direction);
            break;
        case RouteAction::FACE:
            pbf_press_dpad(context, dpad(step.direction), FACE_HOLD, STEP_RELEASE);
            break;
        }
        if (step.action != RouteAction::FACE){
            check_not_interrupted(console, context, step);
        }
        facing = step.walked_out ? MapDirection::SOUTH : step.direction;
    }
    context.wait_for_all_requests();
}



}
}
}
