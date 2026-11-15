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

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  Whether two distance reads agree: within what a few steps cover.
bool reads_agree(int a, int b){
    return a >= b - 12 && a <= b + 12;
}

} // namespace

bool DistanceReads::accept(int distance){
    if (m_last && !reads_agree(distance, *m_last)){
        m_rejected = m_rejected_read && reads_agree(distance, *m_rejected_read) ? m_rejected + 1 : 1;
        m_rejected_read = distance;
        if (m_rejected < 3){
            return false;
        }
    }
    m_rejected_read.reset();
    m_rejected = 0;
    m_last = distance;
    return true;
}


//  level_camera() holds the right stick down 1500 ms to pin the pitch at
//  its lowest, then up 600 ms for the walking pitch (400 ms is still too
//  low, 1000 ms looks at the sky).
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
    DistanceReads reads;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, console.video().snapshot());
        if (marker && turn_to_pinned_marker(context, marker->x)){
            continue;
        }
        if (marker && marker->distance > 0 && reads.accept(marker->distance) && *reads.last() >= stop_at){
            console.log(std::format("walk_from_objective(): {} from the objective.", *reads.last()));
            return reads.last();
        }
        //  Back in the middle before each step: walking toward the camera
        //  turns it, so with no steering the walk from 60 to 100 bent off
        //  the road (held up at 103).
        const CameraTurn turn = marker ? camera_turn_toward(marker->x) : CameraTurn{};
        if (turn.time.count() > 0){
            pbf_move_right_joystick(context, {turn.stick_x, 0}, turn.time, 150ms);
        }
        pbf_move_left_joystick(context, {0, -1}, 600ms, 0ms);
    }
    console.log(std::format("walk_from_objective(): not back to {} in time (last read {}).", stop_at, reads.last().value_or(-1)));
    return reads.last();
}

void check_out_of_wild_zone(ConsoleHandle& console, ProControllerContext& context){
    MinimapZoneDetector zone;
    for (int look = 0; look < 4; look++){
        if (look > 0){
            //  On, away from the gate: just outside it the arrow still
            //  touches the zone's green edge.
            pbf_move_left_joystick(context, {0, 1}, 600ms, 200ms);
        }
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (frame && !zone.detect(frame)){
            console.log(std::format("check_out_of_wild_zone(): the minimap's arrow is out of the zone's green (look {}).", look + 1));
            return;
        }
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
