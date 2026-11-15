/*  PokemonLZA AutoStory - Walks back from the objective marker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <string>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMinimapZoneDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryBracketWatch.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryMarkerWalks.h"
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


//  Console B, 2026-11-14 15:25: the right stick down held 1500 ms pins the
//  pitch at its lowest; from there the stick up held 600 ms gives the
//  walking pitch (400 ms is still too low, 1000 ms looks at the sky).
constexpr Milliseconds CAMERA_PITCH_DOWN_HOLD = 1500ms;
constexpr Milliseconds CAMERA_PITCH_UP_HOLD = 600ms;

void level_camera(ConsoleHandle& console, ProControllerContext& context){
    console.log("level_camera(): pitch reset");
    pbf_move_right_joystick(context, {0, -1}, CAMERA_PITCH_DOWN_HOLD, 100ms);
    pbf_move_right_joystick(context, {0, 1}, CAMERA_PITCH_UP_HOLD, 300ms);
}

bool turn_to_pinned_marker(ProControllerContext& context, double x){
    if (x >= 0.10 && x <= 0.90){
        return false;
    }
    pbf_move_right_joystick(context, {x < 0.5 ? -0.5 : 0.5, 0}, 400ms, 200ms);
    return true;
}

std::optional<int> walk_from_objective(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit){
    const WallClock deadline = current_time() + limit;
    std::optional<int> last;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, console.video().snapshot());
        if (marker && turn_to_pinned_marker(context, marker->x)){
            continue;
        }
        if (marker && marker->distance > 0 && (!last || std::abs(marker->distance - *last) <= 12)){
            last = marker->distance;
            if (*last >= stop_at){
                console.log(std::format("walk_from_objective(): {} from the objective.", *last));
                return last;
            }
        }
        //  Back in the middle before each step: walking toward the camera
        //  turns it, so with no steering the walk from 60 to 100 bent off
        //  the road (console B, 2026-11-14: held up at 103).
        const CameraTurn turn = marker ? camera_turn_toward(marker->x) : CameraTurn{};
        if (turn.time.count() > 0){
            pbf_move_right_joystick(context, {turn.stick_x, 0}, turn.time, 150ms);
        }
        pbf_move_left_joystick(context, {0, -1}, 600ms, 0ms);
    }
    console.log(std::format("walk_from_objective(): not back to {} in time (last read {}).", stop_at, last.value_or(-1)));
    return last;
}

namespace{

//  How many units past its stop distance walk_to_objective() ends on the
//  zone 1 road: 2 on every console B run (asked 137, read 135; asked 127,
//  read 125; 2026-11-14). 03.07d stops this much early so it ends on 127.
constexpr int ZONE_1_START_OVERSHOOT = 2;

//  Where 03.07d's last leg keeps the far gate's marker on screen (0 to 1):
//  right of the middle, so the walk heads about 2.5 degrees left of the
//  gate (the camera shows about 60 degrees across). Straight at the marker
//  ended a bit right of 03.08's start and 5 degrees a bit left (user,
//  console B 2026-11-14 14:33 and 14:41); the user picked the middle.
constexpr double ZONE_1_START_AIM_X = 0.5 + 2.5 / 60.0;

//  Turns the camera, without walking, until the objective marker sits at
//  `x` on screen (15 turns at most). Returns the marker last read.
std::optional<ObjectiveMarker> turn_marker_to(ConsoleHandle& console, ProControllerContext& context, double x){
    std::optional<ObjectiveMarker> marker;
    for (int i = 0; i < 15; i++){
        context.wait_for_all_requests();
        marker = read_objective_marker(console, console.video().snapshot());
        if (!marker){
            return marker;
        }
        const double off = marker->x - x;
        if (std::abs(off) < 0.02){
            return marker;
        }
        pbf_move_right_joystick(context, {off > 0 ? 0.4 : -0.4, 0}, Milliseconds((int)(std::min(1.0, std::abs(off) * 4) * 400)) + 60ms, 150ms);
    }
    return marker;
}

//  Walks with the far gate's marker held at ZONE_1_START_AIM_X until its
//  distance reads `stop_at` or less, or `limit`. A read more than 12 from
//  the last is a misread and skipped. Returns the last distance read.
std::optional<int> walk_left_of_far_gate(ConsoleHandle& console, ProControllerContext& context, int stop_at, Milliseconds limit){
    const WallClock deadline = current_time() + limit;
    std::optional<int> last;
    while (current_time() < deadline){
        const std::optional<ObjectiveMarker> marker = turn_marker_to(console, context, ZONE_1_START_AIM_X);
        if (marker && marker->distance > 0 && (!last || std::abs(marker->distance - *last) <= 12)){
            last = marker->distance;
            if (*last <= stop_at){
                console.log(std::format("walk_to_zone_1_start(): {} to the far gate, aimed left of it.", *last));
                return last;
            }
        }
        pbf_move_left_joystick(context, {0, 1}, 600ms, 0ms);
    }
    console.log(std::format("walk_to_zone_1_start(): not within {} in time (last read {}).", stop_at, last.value_or(-1)));
    return last;
}

} // namespace

void walk_to_zone_1_start(ConsoleHandle& console, ProControllerContext& context){
    walk_left_of_far_gate(console, context, ZONE_1_START_DISTANCE + ZONE_1_START_OVERSHOOT, 60s);
    turn_marker_to(console, context, 0.5);
    context.wait_for_all_requests();
    const VideoSnapshot frame = console.video().snapshot();
    const std::optional<ObjectiveMarker> marker = read_objective_marker(console, frame);
    const std::optional<int> distance = marker && marker->distance > 0 ? std::optional<int>(marker->distance) : std::nullopt;
    MinimapZoneDetector zone;
    const bool in_zone = zone.detect(frame);
    const std::string readings = std::format(
        "the far gate reads {}, the minimap's arrow {} the zone's green",
        distance ? std::to_string(*distance) : "nothing", in_zone ? "in" : "not in"
    );
    if (at_zone_1_start(distance, in_zone)){
        console.log(std::format("walk_to_zone_1_start(): at 03.08's start: {}.", readings));
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("walk_to_zone_1_start(): not at 03.08's start ({} +- {}): {}.", ZONE_1_START_DISTANCE, ZONE_1_START_TOLERANCE, readings),
        console
    );
}

void check_out_of_wild_zone(ConsoleHandle& console, ProControllerContext& context){
    MinimapZoneDetector zone;
    for (int look = 0; look < 4; look++){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && !zone.detect(frame)){
            console.log(std::format("check_out_of_wild_zone(): the minimap's arrow is out of the zone's green (look {}).", look + 1));
            return;
        }
        context.wait_for(500ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        "check_out_of_wild_zone(): the minimap's arrow is still in the wild zone's green after the gate.",
        console
    );
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
