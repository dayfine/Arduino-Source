/*  PokemonLZA AutoStory - Walks back from the objective marker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include <string>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryMinimapZoneDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryMarkerWalks.h"
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


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
        pbf_move_left_joystick(context, {0, -1}, 600ms, 0ms);
    }
    console.log(std::format("walk_from_objective(): not back to {} in time (last read {}).", stop_at, last.value_or(-1)));
    return last;
}

void walk_to_zone_1_start(ConsoleHandle& console, ProControllerContext& context){
    walk_to_objective(console, context, ZONE_1_START_DISTANCE + ZONE_1_START_TOLERANCE, 60s);
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


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
