/*  BDSP AutoStory - Route IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP_AutoStory_LocationBannerWatcher.h"
#include "PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP_AutoStory_WalkScreen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;
using namespace Pokemon;


namespace{

//  Holding toward a wall with people or Pokemon moving beside us never reads
//  as blocked (8 to 16 per 400 ms in the Oreburgh Mine, s03d1), but walking
//  scrolls the view far more (35 to 90). True if holding toward (x, y) for
//  1.6 s changes the screen less than walking would: we stand at a wall.
bool holding_against_wall(VideoStream& stream, ProControllerContext& context, double x, double y, const std::string& label){
    constexpr double WALKING_CHANGE = 25.0;
    context.wait_for_all_requests();
    VideoSnapshot before = stream.video().snapshot();
    double most = 0;
    for (size_t chunk = 0; chunk < 4 && before; chunk++){
        pbf_move_left_joystick(context, {x, y}, 400ms, 0ms);
        context.wait_for_all_requests();
        VideoSnapshot after = stream.video().snapshot();
        if (!after){
            return false;
        }
        most = std::max(most, walk_screen_change(*before.frame, *after.frame));
        before = std::move(after);
    }
    stream.log("[Walk] " + label + ": most screen change holding on " + std::to_string(most) +
        (most < WALKING_CHANGE ? ", less than walking: at the wall." : ", as much as walking: still moving."));
    return before && most < WALKING_CHANGE;
}

} // namespace


WalkResult ConsoleRouteIO::walk_to_wall(const RouteWallWalk& walk){
    return walk_until_dpad(m_stream, m_context, m_escape, route_stick(walk.direction), walk.limit, walk.label, walk.options);
}

WalkResult ConsoleRouteIO::walk_taps(Direction direction, size_t tiles, const std::string& label){
    return walk_tiles_dpad(m_stream, m_context, m_escape, route_stick(direction), tiles, label);
}

void ConsoleRouteIO::press(const RouteInput& input){
    press_route_input(m_context, input);
}

bool ConsoleRouteIO::dialog_within(Milliseconds wait, Color color){
    ShortDialogWatcher dialog(color);
    return wait_until(m_stream, m_context, wait, {{dialog}}) == 0;
}

std::string ConsoleRouteIO::banner_within(Milliseconds wait){
    LocationBannerWatcher banner;
    wait_until(m_stream, m_context, wait, {{banner}});
    return banner.seen();
}

bool ConsoleRouteIO::holding_against_wall(Direction direction, const std::string& label){
    const JoystickPosition stick = route_stick(direction);
    return PokemonBDSP::holding_against_wall(m_stream, m_context, stick.x, stick.y, label);
}

void ConsoleRouteIO::look(const std::function<void(const ImageViewRGB32& screen)>& at){
    m_context.wait_for_all_requests();
    if (VideoSnapshot screen = m_stream.video().snapshot()){
        at(*screen.frame);
    }
}

void ConsoleRouteIO::note(const std::string& text, RouteNote kind){
    switch (kind){
    case RouteNote::LOG:
        m_stream.log(text);
        return;
    case RouteNote::WARNING:
        m_stream.log(text, COLOR_ORANGE);
        return;
    case RouteNote::STATUS:
        route_status(m_stream, text);
        return;
    case RouteNote::FAILURE:
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, text, m_stream);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
