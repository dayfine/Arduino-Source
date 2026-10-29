/*  Overworld Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace std::chrono_literals;


namespace{

//  Not yet calibrated on the console.
constexpr Milliseconds TILE_TIME = 267ms;       //  16 frames at 60 fps
constexpr Milliseconds TURN_TIME = 133ms;       //  turning before the first step
constexpr Milliseconds FACE_HOLD = 40ms;        //  short enough to turn without stepping
constexpr Milliseconds STEP_RELEASE = 100ms;
constexpr Milliseconds WARP_FADE_TIMEOUT = 3000ms;
constexpr Milliseconds WARP_ARRIVAL_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_PROMPT_TIMEOUT = 5000ms;
constexpr Milliseconds SURF_ANIMATION = 3000ms;


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

void start_surf(ConsoleHandle& console, ProControllerContext& context, MapDirection direction){
    //  "The water is a deep blue... Would you like to SURF?", with YES selected.
    pbf_press_dpad(context, dpad(direction), FACE_HOLD, STEP_RELEASE);
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
    pbf_press_button(context, BUTTON_A, 160ms, SURF_ANIMATION);
}

}



void walk_overworld_route(
    ConsoleHandle& console, ProControllerContext& context,
    const std::vector<RouteStep>& route
){
    //  Each leg starts with a turn unless it goes the way the player faces.
    //  The first leg's facing isn't known, so it allows for the turn.
    std::optional<MapDirection> facing;
    for (const RouteStep& step : route){
        console.log("Route: " + to_string(step));
        switch (step.action){
        case RouteAction::WALK:{
            //  Release half a tile before the last step would end: the step
            //  in progress completes, and no further step starts.
            const Milliseconds turn = facing == step.direction ? 0ms : TURN_TIME;
            const Milliseconds hold = turn + TILE_TIME * (step.tiles - 1) + TILE_TIME / 2;
            pbf_press_dpad(context, dpad(step.direction), hold, STEP_RELEASE);
            if (step.warps){
                wait_out_warp(console, context, step);
            }
            break;
        }
        case RouteAction::SURF:
            start_surf(console, context, step.direction);
            break;
        case RouteAction::FACE:
            pbf_press_dpad(context, dpad(step.direction), FACE_HOLD, STEP_RELEASE);
            break;
        }
        facing = step.direction;
    }
    context.wait_for_all_requests();
}



}
}
}
