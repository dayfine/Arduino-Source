/*  PokemonLZA AutoStory - Walks back from the objective marker
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"
#include "PokemonLZA_AutoStoryCatches.h"
#include "PokemonLZA_AutoStoryDialogs.h"
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

namespace{

//  Walks back away from the objective marker until an A prompt or a
//  dialog shows: Wild Zone 1's entrance gate. Returns the last distance
//  read; throws OperationFailedException when none shows in 60 s.
std::optional<int> walk_back_to_prompt(ConsoleHandle& console, ProControllerContext& context){
    PlzaScreenObserver observer;
    const WallClock deadline = current_time() + 60s;
    std::optional<int> last;
    while (current_time() < deadline){
        context.wait_for_all_requests();
        const VideoSnapshot frame = console.video().snapshot();
        if (observer.observe(frame, current_time()).any_of({ScreenEvent::INTERACT_PROMPT, ScreenEvent::DIALOG_BOX})){
            return last;
        }
        const std::optional<ObjectiveMarker> marker = read_objective_marker(console, frame);
        if (marker && turn_to_pinned_marker(context, marker->x)){
            continue;
        }
        if (marker && marker->distance > 0 && last != marker->distance){
            last = marker->distance;
            console.log(std::format("zone_1_back_to_center(): {} from the far gate.", *last));
        }
        pbf_move_left_joystick(context, {0, -1}, 600ms, 0ms);
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format("zone_1_back_to_center(): no \"A Leave\" prompt in 60 s, last read {} from the far gate.", last.value_or(-1)),
        console
    );
}

} // namespace

void zone_1_back_to_center(ConsoleHandle& console, ProControllerContext& context){
    const std::optional<int> gate = walk_back_to_prompt(console, context);
    console.log(std::format("zone_1_back_to_center(): a prompt at {} from the far gate: leaving.", gate.value_or(-1)));
    pbf_press_button(context, BUTTON_A, 160ms, 840ms);
    mash_to_overworld(console, context, BUTTON_B, 60s);
    const std::optional<int> last = walk_from_objective(console, context, CENTER_FROM_FAR_GATE, 40s);
    if (left_zone_1(last)){
        console.log(std::format("zone_1_back_to_center(): {} from the far gate: out of Wild Zone 1.", *last));
        return;
    }
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format(
            "zone_1_back_to_center(): {} from the far gate: not past {}, so 03.08 would take the player as in Wild Zone 1.",
            last.value_or(-1), ZONE_1_START_MAX
        ),
        console
    );
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
